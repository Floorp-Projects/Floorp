/* eslint-disable mozilla/no-comparison-or-assignment-inside-ok */

function ok(a, msg) {
  postMessage({ type: "status", status: !!a, msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish() {
  postMessage({ type: "finish" });
}

function runTests() {
  ok("connection" in navigator, "navigator.connection should exist");
  is(
    navigator.connection.type,
    "unknown",
    "The connection type is spoofed correctly"
  );

  finish();
}

self.onmessage = function (e) {
  if (e.data.type === "runTests") {
    runTests();
  } else {
    ok(false, "Unknown message type");
  }
};
