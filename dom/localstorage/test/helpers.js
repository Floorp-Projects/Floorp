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
      set: [
        ["dom.storage.testing", true],
        ["dom.quotaManager.testing", true],
      ],
    });

    info("Clearing old databases");

    await requestFinished(clearAllDatabases());

    SimpleTest.registerCleanupFunction(async function() {
      await requestFinished(clearAllDatabases());
    });
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

class RequestError extends Error {
  constructor(resultCode, resultName) {
    super(`Request failed (code: ${resultCode}, name: ${resultName})`);
    this.name = "RequestError";
    this.resultCode = resultCode;
    this.resultName = resultName;
  }
}

async function requestFinished(request) {
  await new Promise(function(resolve) {
    request.callback = SpecialPowers.wrapCallback(function() {
      resolve();
    });
  });

  if (request.resultCode !== SpecialPowers.Cr.NS_OK) {
    throw new RequestError(request.resultCode, request.resultName);
  }

  return request.result;
}
