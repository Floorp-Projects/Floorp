importScripts("common_temporaryFileBlob.js");

function info(msg) {
  postMessage({ type: "info", msg: msg });
}

function ok(a, msg) {
  postMessage({ type: "check", what: !!a, msg: msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function next() {
  postMessage({ type: "finish" });
}

onmessage = function(e) {
  if (e.data == "xhr") {
    test_xhr_basic();
  } else if (e.data == "fetch") {
    test_fetch_basic();
  } else if (e.data == "response") {
    test_response_basic();
  } else if (e.data == "request") {
    test_request_basic();
  } else {
    ok(false, "Unknown message");
  }
};
