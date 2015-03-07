// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
//
// This helper script exposes a runTests function that takes the name of a
// test script as its input argument and runs the test in three different
// contexts:
// 1. Regular Worker context
// 2. Service Worker context
// 3. Window context
// The function returns a promise which will get resolved once all tests
// finish. The caller of this function is responsible to call SimpleTest.finish
// when the returned promise is resolved.

function runTests(testFile) {
  function setupPrefs() {
    return new Promise(function(resolve, reject) {
      SpecialPowers.pushPrefEnv({
        "set": [["dom.caches.enabled", true],
                ["dom.serviceWorkers.enabled", true],
                ["dom.serviceWorkers.testing.enabled", true],
                ["dom.serviceWorkers.exemptFromPerDomainMax", true]]
      }, function() {
        resolve();
      });
    });
  }

  function loadScript(script) {
    return new Promise(function(resolve, reject) {
      var s = document.createElement("script");
      s.src = script;
      s.onerror = reject;
      s.onload = resolve;
      document.body.appendChild(s);
    });
  }

  function importDrivers() {
    return Promise.all([loadScript("worker_driver.js"),
                        loadScript("serviceworker_driver.js")]);
  }

  function runWorkerTest() {
    return workerTestExec(testFile);
  }

  function runServiceWorkerTest() {
    return serviceWorkerTestExec(testFile);
  }

  function runFrameTest() {
    return new Promise(function(resolve, reject) {
      var iframe = document.createElement("iframe");
      iframe.src = "frame.html";
      iframe.onload = function() {
        var doc = iframe.contentDocument;
        var s = doc.createElement("script");
        s.src = testFile;
        window.onmessage = function(event) {
          if (event.data.type == 'finish') {
            window.onmessage = null;
            resolve();
          } else if (event.data.type == 'status') {
            ok(event.data.status, event.data.msg);
          }
        };
        doc.body.appendChild(s);
      };
      document.body.appendChild(iframe);
    });
  }

  SimpleTest.waitForExplicitFinish();
  return setupPrefs()
      .then(importDrivers)
      // TODO: Investigate interleaving these tests by making the driver able
      // to differentiate where the incoming messages are coming from and use
      // Promise.all([runWorkerTest, runServiceWorkerTest, runFrameTest]t) below.
      .then(runWorkerTest)
      .then(runServiceWorkerTest)
      .then(runFrameTest)
      .catch(function(e) {
        ok(false, "A promise was rejected during test execution: " + e);
      });
}

