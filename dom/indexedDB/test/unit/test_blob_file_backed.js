/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
let testGenerator = testSteps();

function createFileReader() {
  return SpecialPowers.Cc["@mozilla.org/files/filereader;1"]
                      .createInstance(SpecialPowers.Ci.nsIDOMFileReader);
}

function testSteps()
{
  const fileIOFlags = 0x02 | // PR_WRONLY
                      0x08 | // PR_CREATEFILE
                      0x20;  // PR_TRUNCATE
  const filePerms = 0664;
  const fileData = "abcdefghijklmnopqrstuvwxyz";
  const fileType = "text/plain";

  const databaseName =
    ("window" in this) ? window.location.pathname : "Test";
  const objectStoreName = "foo";
  const objectStoreKey = "10";

  info("Creating temp file");

  let dirSvc =
    SpecialPowers.Cc["@mozilla.org/file/directory_service;1"]
                 .getService(SpecialPowers.Ci.nsIProperties);
  let testFile = dirSvc.get("ProfD", SpecialPowers.Ci.nsIFile);
  testFile.createUnique(SpecialPowers.Ci.nsIFile.NORMAL_FILE_TYPE, filePerms);

  info("Writing temp file");

  let outStream =
    SpecialPowers.Cc["@mozilla.org/network/file-output-stream;1"]
                 .createInstance(SpecialPowers.Ci.nsIFileOutputStream);
  outStream.init(testFile, fileIOFlags, filePerms, 0);
  outStream.write(fileData, fileData.length);
  outStream.close();

  let file = SpecialPowers.createDOMFile(testFile.path, { type: fileType });
  ok(file instanceof File, "Got a File object");
  is(file.size, fileData.length, "Correct size");
  is(file.type, fileType, "Correct type");

  let fileReader = createFileReader();
  fileReader.onload = grabEventAndContinueHandler;
  fileReader.readAsText(file);

  let event = yield undefined;

  is(fileReader.result, fileData, "Correct data");

  let request = indexedDB.open(databaseName, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  let db = event.target.result;
  let objectStore = db.createObjectStore(objectStoreName);
  objectStore.put(file, objectStoreKey);

  event = yield undefined;

  db = event.target.result;

  file = null;
  testFile.remove(false);

  objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);
  objectStore.get(objectStoreKey).onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  file = event.target.result;

  ok(file instanceof File, "Got a File object");
  is(file.size, fileData.length, "Correct size");
  is(file.type, fileType, "Correct type");

  fileReader = createFileReader();
  fileReader.onload = grabEventAndContinueHandler;
  fileReader.readAsText(file);

  event = yield undefined;

  is(fileReader.result, fileData, "Correct data");

  finishTest();
  yield undefined;
}
