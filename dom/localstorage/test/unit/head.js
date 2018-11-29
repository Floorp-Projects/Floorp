/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const NS_ERROR_DOM_QUOTA_EXCEEDED_ERR = 22;

ChromeUtils.import("resource://gre/modules/Services.jsm");

function is(a, b, msg)
{
  Assert.equal(a, b, msg);
}

function ok(cond, msg)
{
  Assert.ok(!!cond, msg);
}

function run_test()
{
  runTest();
};

if (!this.runTest) {
  this.runTest = function()
  {
    do_get_profile();

    enableTesting();

    do_test_pending();
    testGenerator.next();
  }
}

function finishTest()
{
  resetTesting();

  executeSoon(function() {
    do_test_finished();
  })
}

function continueToNextStepSync()
{
  testGenerator.next();
}

function enableTesting()
{
  Services.prefs.setBoolPref("dom.storage.testing", true);
  Services.prefs.setBoolPref("dom.quotaManager.testing", true);
}

function resetTesting()
{
  Services.prefs.clearUserPref("dom.quotaManager.testing");
  Services.prefs.clearUserPref("dom.storage.testing");
}

function setGlobalLimit(globalLimit)
{
  Services.prefs.setIntPref("dom.quotaManager.temporaryStorage.fixedLimit",
                            globalLimit);
}

function resetGlobalLimit()
{
  Services.prefs.clearUserPref("dom.quotaManager.temporaryStorage.fixedLimit");
}

function setOriginLimit(originLimit)
{
  Services.prefs.setIntPref("dom.storage.default_quota", originLimit);
}

function resetOriginLimit()
{
  Services.prefs.clearUserPref("dom.storage.default_quota");
}

function getOriginUsage(principal, callback)
{
  let request = Services.qms.getUsageForPrincipal(principal, callback);
  request.callback = callback;

  return request;
}

function clear(callback)
{
  let request = Services.qms.clear();
  request.callback = callback;

  return request;
}

function resetOrigin(principal, callback)
{
  let request =
    Services.qms.resetStoragesForPrincipal(principal, "default", "ls");
  request.callback = callback;

  return request;
}

function repeatChar(count, ch) {
  if (count == 0) {
    return "";
  }

  let result = ch;
  let count2 = count / 2;

  // Double the input until it is long enough.
  while (result.length <= count2) {
    result += result;
  }

  // Use substring to hit the precise length target without using extra memory.
  return result + result.substring(0, count - result.length);
}

function getPrincipal(url)
{
  let uri = Services.io.newURI(url);
  return Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
}

function getCurrentPrincipal()
{
  return Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);
}

function getLocalStorage(principal)
{
  if (!principal) {
    principal = getCurrentPrincipal();
  }

  return Services.domStorageManager.createStorage(null, principal, "");
}
