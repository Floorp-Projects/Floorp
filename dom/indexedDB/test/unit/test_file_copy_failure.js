/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = "test_file_copy_failure.js";
  const objectStoreName = "Blobs";
  const blob = getBlob(getView(1024));

  info("Opening database");

  let request = indexedDB.open(name);
  request.onerror = errorHandler;
  request.onupgradeneeded = continueToNextStepSync;
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  // upgradeneeded
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = continueToNextStepSync;

  info("Creating objectStore");

  request.result.createObjectStore(objectStoreName);

  yield undefined;

  // success
  let db = request.result;
  db.onerror = errorHandler;

  info("Creating orphaned file");

  let filesDir = getChromeFilesDir();

  let journalFile = filesDir.clone();
  journalFile.append("journals");
  journalFile.append("1");

  let exists = journalFile.exists();
  ok(!exists, "Journal file doesn't exist");

  journalFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));

  let file = filesDir.clone();
  file.append("1");

  exists = file.exists();
  ok(!exists, "File doesn't exist");

  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));

  info("Storing blob");

  let trans = db.transaction(objectStoreName, "readwrite");

  request = trans.objectStore(objectStoreName).add(blob, 1);
  request.onsuccess = continueToNextStepSync;

  yield undefined;

  trans.oncomplete = continueToNextStepSync;

  yield undefined;

  exists = journalFile.exists();
  ok(!exists, "Journal file doesn't exist");

  finishTest();
  yield undefined;
}
