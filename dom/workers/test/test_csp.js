/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var tests = 3;

SimpleTest.waitForExplicitFinish();

testDone = function(event) {
  if (!--tests) SimpleTest.finish();
}

// Workers don't inherit CSP
worker = new Worker("csp_worker.js");
worker.postMessage({ do: "eval" });
worker.onmessage = function(event) {
  is(event.data, 42, "Eval succeeded!");
  testDone();
}

// blob: workers *do* inherit CSP
xhr = new XMLHttpRequest;
xhr.open("GET", "csp_worker.js");
xhr.responseType = "blob";
xhr.send();
xhr.onload = (e) => {
  uri = URL.createObjectURL(e.target.response);
  worker = new Worker(uri);
  worker.postMessage({ do: "eval" })
  worker.onmessage = function(event) {
    is(event.data, "EvalError: call to eval() blocked by CSP", "Eval threw");
    testDone();
  }
}

xhr = new XMLHttpRequest;
xhr.open("GET", "csp_worker.js");
xhr.responseType = "blob";
xhr.send();
xhr.onload = (e) => {
  uri = URL.createObjectURL(e.target.response);
  worker = new Worker(uri);
  worker.postMessage({ do: "nest", uri: uri, level: 3 })
  worker.onmessage = function(event) {
    is(event.data, "EvalError: call to eval() blocked by CSP", "Eval threw in nested worker");
    testDone();
  }
}
