var qrcode = new QRCode(document.getElementById('qr-code'), {
    width: 208,
    height: 208,
	correctLevel : QRCode.CorrectLevel.H
});
var makeQr = (tabs) => {
    var url = tabs[0].url;
    qrcode.makeCode(url);
};
browser.tabs.query({ currentWindow: true, active: true }).then(makeQr);