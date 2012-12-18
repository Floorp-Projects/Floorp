/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const DEFAULT_QUOTA = 50 * 1024 * 1024;

var bufferCache = [];
var utils = SpecialPowers.getDOMWindowUtils(window);

if (!SpecialPowers.isMainProcess()) {
  window.runTest = function() {
    todo(false, "Test disabled in child processes, for now");
    finishTest();
  }
}

function getView(size)
{
  let buffer = new ArrayBuffer(size);
  let view = new Uint8Array(buffer);
  is(buffer.byteLength, size, "Correct byte length");
  return view;
}

function getRandomView(size)
{
  let view = getView(size);
  for (let i = 0; i < size; i++) {
    view[i] = parseInt(Math.random() * 255)
  }
  return view;
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

function getBlob(type, view)
{
  return utils.getBlob([view], {type: type});
}

function getFile(name, type, view)
{
  return utils.getFile(name, [view], {type: type});
}

function getRandomBlob(size)
{
  return getBlob("binary/random", getRandomView(size));
}

function getRandomFile(name, size)
{
  return getFile(name, "binary/random", getRandomView(size));
}

function getNullBlob(size)
{
  return getBlob("binary/null", getView(size));
}

function getNullFile(name, size)
{
  return getFile(name, "binary/null", getView(size));
}

function verifyBuffers(buffer1, buffer2)
{
  ok(compareBuffers(buffer1, buffer2), "Correct blob data");
}

function verifyBlob(blob1, blob2, fileId, blobReadHandler)
{
  is(blob1 instanceof Components.interfaces.nsIDOMBlob, true,
     "Instance of nsIDOMBlob");
  is(blob1 instanceof Components.interfaces.nsIDOMFile,
     blob2 instanceof Components.interfaces.nsIDOMFile,
     "Instance of nsIDOMFile");
  is(blob1.size, blob2.size, "Correct size");
  is(blob1.type, blob2.type, "Correct type");
  if (blob2 instanceof Components.interfaces.nsIDOMFile) {
    is(blob1.name, blob2.name, "Correct name");
  }
  is(utils.getFileId(blob1), fileId, "Correct file id");

  let buffer1;
  let buffer2;

  for (let i = 0; i < bufferCache.length; i++) {
    if (bufferCache[i].blob == blob2) {
      buffer2 = bufferCache[i].buffer;
      break;
    }
  }

  if (!buffer2) {
    let reader = new FileReader();
    reader.readAsArrayBuffer(blob2);
    reader.onload = function(event) {
      buffer2 = event.target.result;
      bufferCache.push({ blob: blob2, buffer: buffer2 });
      if (buffer1) {
        verifyBuffers(buffer1, buffer2);
        if (blobReadHandler) {
          blobReadHandler();
        }
        else {
          testGenerator.next();
        }
      }
    }
  }

  let reader = new FileReader();
  reader.readAsArrayBuffer(blob1);
  reader.onload = function(event) {
    buffer1 = event.target.result;
    if (buffer2) {
      verifyBuffers(buffer1, buffer2);
      if (blobReadHandler) {
        blobReadHandler();
      }
      else {
        testGenerator.next();
      }
    }
  }
}

function verifyBlobArray(blobs1, blobs2, expectedFileIds)
{
  is(blobs1 instanceof Array, true, "Got an array object");
  is(blobs1.length, blobs2.length, "Correct length");

  if (!blobs1.length) {
    return;
  }

  let verifiedCount = 0;

  function blobReadHandler() {
    if (++verifiedCount == blobs1.length) {
      testGenerator.next();
    }
    else {
      verifyBlob(blobs1[verifiedCount], blobs2[verifiedCount],
                 expectedFileIds[verifiedCount], blobReadHandler);
    }
  }

  verifyBlob(blobs1[verifiedCount], blobs2[verifiedCount],
             expectedFileIds[verifiedCount], blobReadHandler);
}

function grabFileUsageAndContinueHandler(usage, fileUsage)
{
  testGenerator.send(fileUsage);
}

function getUsage(usageHandler)
{
  let comp = SpecialPowers.wrap(Components);
  let idbManager = comp.classes["@mozilla.org/dom/indexeddb/manager;1"]
                       .getService(comp.interfaces.nsIIndexedDatabaseManager);

  let uri = SpecialPowers.getDocumentURIObject(window.document);
  let callback = {
    onUsageResult: function(uri, usage, fileUsage) {
      usageHandler(usage, fileUsage);
    }
  };

  idbManager.getUsageForURI(uri, callback);
}

function scheduleGC()
{
  SpecialPowers.exactGC(window, continueToNextStep);
}

function getFileId(file)
{
  return utils.getFileId(file);
}

function hasFileInfo(name, id)
{
  return utils.getFileReferences(name, id);
}

function getFileRefCount(name, id)
{
  let count = {};
  utils.getFileReferences(name, id, count);
  return count.value;
}

function getFileDBRefCount(name, id)
{
  let count = {};
  utils.getFileReferences(name, id, {}, count);
  return count.value;
}
