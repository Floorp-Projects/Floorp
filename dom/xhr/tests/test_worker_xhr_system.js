/* eslint-disable mozilla/no-comparison-or-assignment-inside-ok */

function ok(what, msg) {
  postMessage({ event: msg, test: "ok", a: what });
}

function is(a, b, msg) {
  postMessage({ event: msg, test: "is", a, b });
}

self.onmessage = function onmessage() {
  // An XHR with system privileges will be able to do cross-site calls.

  const TEST_URL =
    "http://example.com/tests/dom/xhr/tests/test_XHR_system.html";
  is(location.hostname, "mochi.test", "hostname should be mochi.test");

  var xhr = new XMLHttpRequest({ mozSystem: true });
  is(xhr.mozSystem, true, ".mozSystem == true");
  xhr.open("GET", TEST_URL);
  xhr.onload = function onload() {
    is(xhr.status, 200);
    ok(xhr.responseText != null);
    ok(xhr.responseText.length);
    postMessage({ test: "finish" });
  };
  xhr.onerror = function onerror() {
    ok(false, "Got an error event!");
    postMessage({ test: "finish" });
  };
  xhr.send();
};
