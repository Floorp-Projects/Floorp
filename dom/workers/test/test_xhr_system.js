function ok(what, msg) {
  postMessage({ event: msg, test: 'ok', a: what });
}

function is(a, b, msg) {
  postMessage({ event: msg, test: 'is', a: a, b: b });
}

self.onmessage = function onmessage(event) {

  // An XHR with system privileges will be able to do cross-site calls.

  const TEST_URL = "http://example.com/tests/content/base/test/test_XHR_system.html";
  is(location.hostname, "mochi.test", "hostname");

  var xhr = new XMLHttpRequest({mozSystem: true});
  is(xhr.mozSystem, true, ".mozSystem == true");
  xhr.open("GET", TEST_URL);
  xhr.onload = function onload() {
    is(xhr.status, 200);
    ok(xhr.responseText != null);
    ok(xhr.responseText.length);
    postMessage({test: "finish"});
  };
  xhr.onerror = function onerror() {
    ok(false, "Got an error event!");
    postMessage({test: "finish"});
  }
  xhr.send();
};
