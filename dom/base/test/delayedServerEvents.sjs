// this will take strings_to_send.length*500 ms = 5 sec

var timer = null;
var strings_to_send = [
  "retry:999999999\ndata\r\n\nda",
  "ta",
  ":",
  "de",
  "layed1\n\n",
  "",
  "",
  "data:delayed2\n\n",
  "",
  "",
];
var resp = null;

function sendNextString() {
  if (!strings_to_send.length) {
    timer.cancel();
    resp.finish();
    timer = null;
    resp = null;
    return;
  }

  try {
    resp.write(strings_to_send.shift());
  } catch (e) {
    timer.cancel();
    timer = null;
    resp = null;
  }
}

function handleRequest(request, response) {
  var bytes = strings_to_send.reduce((len, s) => len + s.length, 0);

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write(`Content-Length: ${bytes}\r\n`);
  response.write("Content-Type: text/event-stream; charset=utf-8\r\n");
  response.write("Cache-Control: no-cache, must-revalidate\r\n");
  response.write("\r\n");

  resp = response;

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(sendNextString, 500, Ci.nsITimer.TYPE_REPEATING_SLACK);
}
