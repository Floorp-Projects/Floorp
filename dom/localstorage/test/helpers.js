/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// testSteps is expected to be defined by the test using this file.
/* global testSteps:false */

function executeSoon(aFun) {
  SpecialPowers.Services.tm.dispatchToMainThread({
    run() {
      aFun();
    },
  });
}

function clearAllDatabases() {
  let qms = SpecialPowers.Services.qms;
  let principal = SpecialPowers.wrap(document).nodePrincipal;
  let request = qms.clearStoragesForPrincipal(principal);
  return request;
}

if (!window.runTest) {
  window.runTest = async function() {
    SimpleTest.waitForExplicitFinish();

    info("Pushing preferences");

    await SpecialPowers.pushPrefEnv({
      set: [["dom.storage.testing", true], ["dom.quotaManager.testing", true]],
    });

    info("Clearing old databases");

    await requestFinished(clearAllDatabases());

    ok(typeof testSteps === "function", "There should be a testSteps function");
    ok(
      testSteps.constructor.name === "AsyncFunction",
      "testSteps should be an async function"
    );

    SimpleTest.registerCleanupFunction(async function() {
      await requestFinished(clearAllDatabases());
    });

    add_task(testSteps);
  };
}

function returnToEventLoop() {
  return new Promise(function(resolve) {
    executeSoon(resolve);
  });
}

function getLocalStorage() {
  return localStorage;
}

function requestFinished(request) {
  return new Promise(function(resolve, reject) {
    request.callback = SpecialPowers.wrapCallback(function(requestInner) {
      if (requestInner.resultCode === SpecialPowers.Cr.NS_OK) {
        resolve(requestInner.result);
      } else {
        reject(requestInner.resultCode);
      }
    });
  });
}
