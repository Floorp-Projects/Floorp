/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function executeSoon(aFun)
{
  SimpleTest.executeSoon(aFun);
}

function runTest()
{
  allowIndexedDB();
  allowUnlimitedQuota();

  SimpleTest.waitForExplicitFinish();
  testGenerator.next();
}

function finishTest()
{
  resetUnlimitedQuota();
  resetIndexedDB();

  SimpleTest.executeSoon(function() {
    testGenerator.close();
    SimpleTest.finish();
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
  ok(false, "indexedDB error, code " + event.target.errorCode);
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

function ExpectError(code)
{
  this._code = code;
}
ExpectError.prototype = {
  handleEvent: function(event)
  {
    is(event.type, "error", "Got an error event");
    is(this._code, event.target.errorCode, "Expected error was thrown.");
    event.preventDefault();
    event.stopPropagation();
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
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  let uri;
  if (url) {
    uri = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService)
                    .newURI(url, null, null);
  }
  else {
    uri = SpecialPowers.getDocumentURIObject(window.document);
  }

  let permission;
  if (allow) {
    permission = Components.interfaces.nsIPermissionManager.ALLOW_ACTION;
  }
  else {
    permission = Components.interfaces.nsIPermissionManager.DENY_ACTION;
  }

  Components.classes["@mozilla.org/permissionmanager;1"]
            .getService(Components.interfaces.nsIPermissionManager)
            .add(uri, type, permission);
}

function removePermission(permission, url)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  let uri;
  if (url) {
    uri = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService)
                    .newURI(url, null, null);
  }
  else {
    uri = SpecialPowers.getDocumentURIObject(window.document);
  }

  Components.classes["@mozilla.org/permissionmanager;1"]
            .getService(Components.interfaces.nsIPermissionManager)
            .remove(uri.host, permission);
}

function setQuota(quota)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);

  prefs.setIntPref("dom.indexedDB.warningQuota", quota);
}

function allowIndexedDB(url)
{
  addPermission("indexedDB", true, url);
}

function resetIndexedDB(url)
{
  removePermission("indexedDB", url);
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
