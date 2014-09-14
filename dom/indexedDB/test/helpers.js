/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();
var archiveReaderEnabled = false;

// The test js is shared between xpcshell (which has no SpecialPowers object)
// and content mochitests (where the |Components| object is accessible only as
// SpecialPowers.Components). Expose Components if necessary here to make things
// work everywhere.
//
// Even if the real |Components| doesn't exist, we might shim in a simple JS
// placebo for compat. An easy way to differentiate this from the real thing
// is whether the property is read-only or not.
var c = Object.getOwnPropertyDescriptor(this, 'Components');
if ((!c.value || c.writable) && typeof SpecialPowers === 'object')
  Components = SpecialPowers.Components;

function executeSoon(aFun)
{
  let comp = SpecialPowers.wrap(Components);

  let thread = comp.classes["@mozilla.org/thread-manager;1"]
                   .getService(comp.interfaces.nsIThreadManager)
                   .mainThread;

  thread.dispatch({
    run: function() {
      aFun();
    }
  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);
}

function clearAllDatabases(callback) {
  let principal = SpecialPowers.wrap(document).nodePrincipal;
  let appId, inBrowser;
  if (principal.appId != Components.interfaces.nsIPrincipal.UNKNOWN_APP_ID &&
      principal.appId != Components.interfaces.nsIPrincipal.NO_APP_ID) {
    appId = principal.appId;
    inBrowser = principal.isInBrowserElement;
  }
  SpecialPowers.clearStorageForURI(document.documentURI, callback, appId, inBrowser);
}

if (!window.runTest) {
  window.runTest = function(limitedQuota)
  {
    SimpleTest.waitForExplicitFinish();

    if (limitedQuota) {
      denyUnlimitedQuota();
    }
    else {
      allowUnlimitedQuota();
    }

    enableTesting();
    enableExperimental();
    enableArchiveReader();

    clearAllDatabases(function () { testGenerator.next(); });
  }
}

function finishTest()
{
  resetUnlimitedQuota();
  resetExperimental();
  resetArchiveReader();
  resetTesting();
  SpecialPowers.notifyObserversInParentProcess(null, "disk-space-watcher",
                                               "free");

  SimpleTest.executeSoon(function() {
    testGenerator.close();
    clearAllDatabases(function() { SimpleTest.finish(); });
  });
}

function browserRunTest()
{
  testGenerator.next();
}

function browserFinishTest()
{
  setTimeout(function() { testGenerator.close(); }, 0);
}

function grabEventAndContinueHandler(event)
{
  testGenerator.send(event);
}

function continueToNextStep()
{
  SimpleTest.executeSoon(function() {
    testGenerator.next();
  });
}

function continueToNextStepSync()
{
  testGenerator.next();
}

function errorHandler(event)
{
  ok(false, "indexedDB error, '" + event.target.error.name + "'");
  finishTest();
}

function browserErrorHandler(event)
{
  browserFinishTest();
  throw new Error("indexedDB error (" + event.code + "): " + event.message);
}

function unexpectedSuccessHandler()
{
  ok(false, "Got success, but did not expect it!");
  finishTest();
}

function expectedErrorHandler(name)
{
  return function(event) {
    is(event.type, "error", "Got an error event");
    is(event.target.error.name, name, "Expected error was thrown.");
    event.preventDefault();
    grabEventAndContinueHandler(event);
  };
}

function ExpectError(name, preventDefault)
{
  this._name = name;
  this._preventDefault = preventDefault;
}
ExpectError.prototype = {
  handleEvent: function(event)
  {
    is(event.type, "error", "Got an error event");
    is(event.target.error.name, this._name, "Expected error was thrown.");
    if (this._preventDefault) {
      event.preventDefault();
      event.stopPropagation();
    }
    grabEventAndContinueHandler(event);
  }
};

function compareKeys(k1, k2) {
  let t = typeof k1;
  if (t != typeof k2)
    return false;

  if (t !== "object")
    return k1 === k2;

  if (k1 instanceof Date) {
    return (k2 instanceof Date) &&
      k1.getTime() === k2.getTime();
  }

  if (k1 instanceof Array) {
    if (!(k2 instanceof Array) ||
        k1.length != k2.length)
      return false;

    for (let i = 0; i < k1.length; ++i) {
      if (!compareKeys(k1[i], k2[i]))
        return false;
    }

    return true;
  }

  return false;
}

function addPermission(type, allow, url)
{
  if (!url) {
    url = window.document;
  }
  SpecialPowers.addPermission(type, allow, url);
}

function removePermission(type, url)
{
  if (!url) {
    url = window.document;
  }
  SpecialPowers.removePermission(type, url);
}

function allowUnlimitedQuota(url)
{
  addPermission("indexedDB-unlimited", true, url);
}

function denyUnlimitedQuota(url)
{
  addPermission("indexedDB-unlimited", false, url);
}

function resetUnlimitedQuota(url)
{
  removePermission("indexedDB-unlimited", url);
}

function enableArchiveReader()
{
  archiveReaderEnabled = SpecialPowers.getBoolPref("dom.archivereader.enabled");
  SpecialPowers.setBoolPref("dom.archivereader.enabled", true);
}

function resetArchiveReader()
{
  SpecialPowers.setBoolPref("dom.archivereader.enabled", archiveReaderEnabled);
}

function enableExperimental()
{
  SpecialPowers.setBoolPref("dom.indexedDB.experimental", true);
}

function resetExperimental()
{
  SpecialPowers.clearUserPref("dom.indexedDB.experimental");
}

function enableTesting()
{
  SpecialPowers.setBoolPref("dom.indexedDB.testing", true);
}

function resetTesting()
{
  SpecialPowers.clearUserPref("dom.indexedDB.testing");
}

function gc()
{
  SpecialPowers.forceGC();
  SpecialPowers.forceCC();
}

function scheduleGC()
{
  SpecialPowers.exactGC(window, continueToNextStep);
}
