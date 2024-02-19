/* eslint-env worker */

/* eslint-disable mozilla/no-comparison-or-assignment-inside-ok */

importScripts("common_temporaryFileBlob.js");

function info(msg) {
  postMessage({ type: "info", msg });
}

function ok(a, msg) {
  postMessage({ type: "check", what: !!a, msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function next() {
  postMessage({ type: "finish" });
}

onmessage = function (e) {
  if (e.data == "simple") {
    test_simple();
  } else if (e.data == "abort") {
    test_abort();
  } else if (e.data == "reuse") {
    test_reuse();
  } else {
    ok(false, "Something wrong happened");
  }
};
