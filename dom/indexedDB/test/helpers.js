/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function executeSoon(aFun)
{
  SimpleTest.executeSoon(aFun);
}

if (!window.runTest) {
  window.runTest = function()
  {
    allowIndexedDB();
    allowUnlimitedQuota();

    SimpleTest.waitForExplicitFinish();
    testGenerator.next();
  }
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

function ExpectError(name)
{
  this._name = name;
}
ExpectError.prototype = {
  handleEvent: function(event)
  {
    is(event.type, "error", "Got an error event");
    is(event.target.error.name, this._name, "Expected error was thrown.");
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

function setQuota(quota)
{
  SpecialPowers.setIntPref("dom.indexedDB.warningQuota", quota);
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
