/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function runTest()
{
  allowIndexedDB();

  SimpleTest.waitForExplicitFinish();
  testGenerator.next();
}

function finishTest()
{
  disallowIndexedDB();

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

function errorHandler(event)
{
  ok(false, "indexedDB error (" + event.code + "): " + event.message);
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
    is(this._code, event.code, "Expected error was thrown.");
    grabEventAndContinueHandler(event);
  }
};

function addPermission(permission, url)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  let uri;
  if (url) {
    uri = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService)
                    .newURI(url, null, null);
  }
  else {
    uri = window.document.documentURIObject;
  }

  Components.classes["@mozilla.org/permissionmanager;1"]
            .getService(Components.interfaces.nsIPermissionManager)
            .add(uri, permission,
                 Components.interfaces.nsIPermissionManager.ALLOW_ACTION);
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
    uri = window.document.documentURIObject;
  }

  Components.classes["@mozilla.org/permissionmanager;1"]
            .getService(Components.interfaces.nsIPermissionManager)
            .remove(uri, permission);
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
  addPermission("indexedDB", url);
}

function disallowIndexedDB(url)
{
  removePermission("indexedDB", url);
}

function allowUnlimitedQuota(url)
{
  addPermission("indexedDB-unlimited", url);
}

function disallowUnlimitedQuota(url)
{
  removePermission("indexedDB-unlimited", url);
}
