/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

var testResult;

function clearAllDatabases(callback) {
  let qms = SpecialPowers.Services.qms;
  let principal = SpecialPowers.wrap(document).nodePrincipal;
  let request = qms.clearStoragesForPrincipal(principal);
  let cb = SpecialPowers.wrapCallback(callback);
  request.callback = cb;
}

function runTest() {
  clearAllDatabases(() => {
    testGenerator.next();
  });
}

function finishTestNow() {
  if (testGenerator) {
    testGenerator.return();
    testGenerator = undefined;
  }
}

function finishTest() {
  clearAllDatabases(() => {
    setTimeout(finishTestNow, 0);
    setTimeout(() => {
      window.parent.postMessage(testResult, "*");
    }, 0);
  });
}

function continueToNextStep() {
  setTimeout(() => {
    testGenerator.next();
  }, 0);
}
