function info(msg) {
  postMessage({type: 'info', msg: msg});
}

function ok(a, msg) {
  postMessage({type: 'check', what: !!a, msg: msg});
}

function finish() {
  postMessage({type: 'finish'});
}

info("Creating XHR...");
var xhr = new XMLHttpRequest();
xhr.open("POST", "echo.sjs");
xhr.responseType = 'arraybuffer';

info("Sending some data...");
var data = new Array(256).join("1234567890ABCDEF");
xhr.send({toString: function() { return data; }});

var aborted = false;

xhr.onprogress = function() {
  info("Onprogress, we abort!");
  aborted = true;
  xhr.abort();
}

xhr.onloadend = function() {
  ok(aborted, "We are still alive after an abort()!");
  finish();
}
