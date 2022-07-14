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
// finish.  The testFile argument is the name of the test file to be run
// in the different contexts, and the optional order argument can be set
// to either "parallel" or "sequential" depending on how the caller wants
// the tests to be run.  If this argument is not provided, the default is
// "both", which runs the tests in both modes.
// The caller of this function is responsible to call SimpleTest.finish
// when the returned promise is resolved.

function runTests(testFile, order) {
  async function setupPrefs() {
    // Bug 1746646: Make mochitests work with TCP enabled (cookieBehavior = 5)
    // Acquire storage access permission here so that the Cache API is avaialable
    SpecialPowers.wrap(document).notifyUserGestureActivation();
    await SpecialPowers.addPermission(
      "storageAccessAPI",
      true,
      window.location.href
    );
    await SpecialPowers.wrap(document).requestStorageAccess();
    return SpecialPowers.pushPrefEnv({
      set: [
        ["dom.caches.enabled", true],
        ["dom.caches.testing.enabled", true],
        ["dom.serviceWorkers.enabled", true],
        ["dom.serviceWorkers.testing.enabled", true],
        ["dom.serviceWorkers.exemptFromPerDomainMax", true],
        [
          "privacy.partition.always_partition_third_party_non_cookie_storage",
          false,
        ],
      ],
    });
  }

  // adapted from dom/indexedDB/test/helpers.js
  function clearStorage() {
    var clearUnpartitionedStorage = new Promise(function(resolve, reject) {
      var qms = SpecialPowers.Services.qms;
      var principal = SpecialPowers.wrap(document).nodePrincipal;
      var request = qms.clearStoragesForPrincipal(principal);
      var cb = SpecialPowers.wrapCallback(resolve);
      request.callback = cb;
    });
    var clearPartitionedStorage = new Promise(function(resolve, reject) {
      var qms = SpecialPowers.Services.qms;
      var principal = SpecialPowers.wrap(document).partitionedPrincipal;
      var request = qms.clearStoragesForPrincipal(principal);
      var cb = SpecialPowers.wrapCallback(resolve);
      request.callback = cb;
    });
    return Promise.all([clearUnpartitionedStorage, clearPartitionedStorage]);
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
    /* import-globals-from worker_driver.js */
    /* import-globals-from serviceworker_driver.js */
    return Promise.all([
      loadScript("worker_driver.js"),
      loadScript("serviceworker_driver.js"),
    ]);
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
        window.addEventListener("message", function onMessage(event) {
          if (event.data.context != "Window") {
            return;
          }
          if (event.data.type == "finish") {
            window.removeEventListener("message", onMessage);
            resolve();
          } else if (event.data.type == "status") {
            ok(event.data.status, event.data.context + ": " + event.data.msg);
          }
        });
        doc.body.appendChild(s);
      };
      document.body.appendChild(iframe);
    });
  }

  SimpleTest.waitForExplicitFinish();

  if (typeof order == "undefined") {
    order = "sequential"; // sequential by default, see bug 1143222.
    // TODO: Make this "both" again.
  }

  ok(
    order == "parallel" || order == "sequential" || order == "both",
    "order argument should be valid"
  );

  if (order == "both") {
    info("Running tests in both modes; first: sequential");
    return runTests(testFile, "sequential").then(function() {
      info("Running tests in parallel mode");
      return runTests(testFile, "parallel");
    });
  }
  if (order == "sequential") {
    return setupPrefs()
      .then(importDrivers)
      .then(runWorkerTest)
      .then(clearStorage)
      .then(runServiceWorkerTest)
      .then(clearStorage)
      .then(runFrameTest)
      .then(clearStorage)
      .catch(function(e) {
        ok(false, "A promise was rejected during test execution: " + e);
      });
  }
  return setupPrefs()
    .then(importDrivers)
    .then(() =>
      Promise.all([runWorkerTest(), runServiceWorkerTest(), runFrameTest()])
    )
    .then(clearStorage)
    .catch(function(e) {
      ok(false, "A promise was rejected during test execution: " + e);
    });
}
