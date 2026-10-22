const express = require('express');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const app = express();

app.use(express.urlencoded({ extended: true }));
app.use(express.json());

const SECRET_KEY = "CSE_DEPT_BLOCKCHAIN_ATTENDANCE";
const LOG_FILE = path.join(__dirname, 'attendance_records.csv');

// Initialize a CSV log file if it doesn't exist
if (!fs.existsSync(LOG_FILE)) {
    fs.writeFileSync(LOG_FILE, "Timestamp,Roll Number,Token Used\n");
}

// 1. Math Function: Generate unique token for a specific 10-second window
function getValidToken(timeOffset = 0) {
    const timeBlock = Math.floor(Date.now() / 10000) + timeOffset; // 10000ms = 10s
    return crypto.createHmac('sha256', SECRET_KEY).update(timeBlock.toString()).digest('hex').substring(0, 8);
}

// 2. Secure Endpoint: Sends the live token to the teacher dashboard
app.get('/api/get-token', (req, res) => {
    res.json({ token: getValidToken(0), serverTime: Date.now() });
});

// 3. Teacher Dashboard UI
app.get('/teacher', (req, res) => {
    res.send(`
    <!DOCTYPE html>
    <html>
    <head>
        <title>Teacher Panel - QR Rotation</title>
        <script src="https://cloudflare.com"></script>
        <style>
            body { font-family: Arial, sans-serif; text-align: center; background: #0f172a; color: white; padding-top: 50px; }
            .card { background: #1e293b; padding: 40px; border-radius: 12px; display: inline-block; box-shadow: 0 4px 20px rgba(0,0,0,0.5); }
            #qrcode { display: flex; justify-content: center; margin: 25px 0; background: white; padding: 15px; border-radius: 8px; }
            .timer { font-size: 20px; color: #f43f5e; font-weight: bold; }
        </style>
    </head>
    <body>
        <div class="card">
            <h2>📊 CSE Live Attendance Scanner</h2>
            <p>Scan using your mobile data. Code updates every 10 seconds.</p>
            <div id="qrcode"></div>
            <div class="timer">Refreshing in: <span id="secs">10</span>s</div>
        </div>
        <script>
            let countdown = 10;
            const timerText = document.getElementById('secs');
            const qrBox = document.getElementById('qrcode');
            let qrcode = new QRCode(qrBox, { width: 250, height: 250 });

            async function updateQR() {
                try {
                    const host = window.location.origin;
                    const res = await fetch(host + '/api/get-token');
                    const data = await res.json();
                    
                    qrBox.innerHTML = "";
                    qrcode = new QRCode(qrBox, { width: 250, height: 250 });
                    // Links student directly to this server's registration portal
                    qrcode.makeCode(host + '/student?token=' + data.token);
                } catch(e) { console.error("Sync error", e); }
            }

            setInterval(() => {
                countdown--;
                timerText.innerText = countdown;
                if(countdown <= 0) {
                    updateQR();
                    countdown = 10;
                }
            }, 1000);
            updateQR();
        </script>
    </body>
    </html>
    `);
});

// 4. Student Registration UI
app.get('/student', (req, res) => {
    const clientToken = req.query.token;
    res.send(`
    <!DOCTYPE html>
    <html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Student Registration</title>
        <style>
            body { font-family: Arial, sans-serif; background: #f8fafc; padding: 20px; display: flex; justify-content: center; align-items: center; height: 80vh; }
            .box { background: white; padding: 30px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); width: 100%; max-width: 400px; text-align: center; }
            input { width: 90%; padding: 12px; margin: 15px 0; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 16px; text-align: center; }
            button { width: 96%; background: #2563eb; color: white; border: none; padding: 12px; font-size: 16px; font-weight: bold; border-radius: 6px; cursor: pointer; }
        </style>
    </head>
    <body>
        <div class="box">
            <h2>📝 Mark Attendance</h2>
            <p>Enter your formal university roll number</p>
            <form action="/submit" method="POST">
                <input type="text" name="rollNumber" placeholder="e.g., 0101CS221005" required>
                <input type="hidden" name="token" value="${clientToken}">
                <button type="submit">Submit</button>
            </form>
        </div>
    </body>
    </html>
    `);
});

// 5. Backend Verification & Logging Engine
app.post('/submit', (req, res) => {
    const { rollNumber, token } = req.body;
    const cleanRoll = rollNumber.trim().toUpperCase();

    // Generate tokens for active window, plus immediate past windows to clear mobile network lag
    const t0 = getValidToken(0);
    const t1 = getValidToken(-1);
    const t2 = getValidToken(-2);

    // Anti-Cheat: Evaluate if token falls inside the acceptable network latency limits
    if (token !== t0 && token !== t1 && token !== t2) {
        return res.status(403).send("<h1>❌ Verification Failed: QR code expired. Please look at the live screen and scan again.</h1>");
    }

    // Read file data to block double entries
    const fileContent = fs.readFileSync(LOG_FILE, 'utf8');
    if (fileContent.includes(cleanRoll)) {
        return res.send("<h1>⚠️ Attendance Alert: You have already marked your presence today.</h1>");
    }

    // Append to CSV
    const timestamp = new Date().toLocaleString("en-IN", { timeZone: "Asia/Kolkata" });
    fs.appendFileSync(LOG_FILE, `"${timestamp}","${cleanRoll}","${token}"\n`);

    res.send("<h1>✅ Success: Your attendance has been securely logged!</h1>");
});

// Admin Route to view the spreadsheet directly via web browser
app.get('/admin/view-logs', (req, res) => {
    const data = fs.readFileSync(LOG_FILE, 'utf8');
    res.type('text/plain').send(data);
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => console.log(`Attendance Engine live on port ${PORT}`));
