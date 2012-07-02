/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const IndexedDatabaseKey = "IDB";
const DeviceStorageKey = "DS";

var fileStorages = [
  { key: IndexedDatabaseKey }
//  { key: DeviceStorageKey }
];

var utils = SpecialPowers.getDOMWindowUtils(window);

var testGenerator = testSteps();

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
  ok(false, "indexedDB error, code " + event.target.errorCode);
  finishTest();
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

function resetUnlimitedQuota(url)
{
  removePermission("indexedDB-unlimited", url);
}

function getFileHandle(fileStorageKey, name)
{
  var requestService = SpecialPowers.getDOMRequestService();
  var request = requestService.createRequest(window);

  switch (fileStorageKey) {
    case IndexedDatabaseKey:
      var dbname = window.location.pathname;
      indexedDB.open(dbname, 1).onsuccess = function(event) {
        var db = event.target.result;
        db.mozCreateFileHandle(name).onsuccess = function(event) {
          var fileHandle = event.target.result;
          requestService.fireSuccess(request, fileHandle);
        }
      }
      break;

    case DeviceStorageKey:
      var dbname = window.location.pathname;
      indexedDB.open(dbname, 1).onsuccess = function(event) {
        var db = event.target.result;
        db.mozCreateFileHandle(name).onsuccess = function(event) {
          var fileHandle = event.target.result;
          requestService.fireSuccess(request, fileHandle);
        }
      }
      break;
  }

  return request;
}

function getBuffer(size)
{
  let buffer = new ArrayBuffer(size);
  is(buffer.byteLength, size, "Correct byte length");
  return buffer;
}

function getRandomBuffer(size)
{
  let buffer = getBuffer(size);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < size; i++) {
    view[i] = parseInt(Math.random() * 255)
  }
  return buffer;
}

function compareBuffers(buffer1, buffer2)
{
  if (buffer1.byteLength != buffer2.byteLength) {
    return false;
  }
  let view1 = new Uint8Array(buffer1);
  let view2 = new Uint8Array(buffer2);
  for (let i = 0; i < buffer1.byteLength; i++) {
    if (view1[i] != view2[i]) {
      return false;
    }
  }
  return true;
}

function getBlob(type, buffer)
{
  return utils.getBlob([buffer], {type: type});
}

function getRandomBlob(size)
{
  return getBlob("binary/random", getRandomBuffer(size));
}

function getFileId(blob)
{
  return utils.getFileId(blob);
}
