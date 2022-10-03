/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function getTestingFiles() {
  const filenameBase = "3128029391StpsleeTn+ddi";
  let baseDir = getRelativeFile("storage/permanent/chrome/idb");

  let dbFile = baseDir.clone();
  dbFile.append(filenameBase + ".sqlite");

  let dir = baseDir.clone();
  dir.append(filenameBase + ".files");

  let markerFile = baseDir.clone();
  markerFile.append("idb-deleting-" + filenameBase);

  return { dbFile, dir, markerFile };
}

function createTestingEnvironment(markerFileOnly = false) {
  let testingFiles = getTestingFiles();

  if (!markerFileOnly) {
    testingFiles.dbFile.create(
      Ci.nsIFile.NORMAL_FILE_TYPE,
      parseInt("0644", 8)
    );

    testingFiles.dir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
  }

  testingFiles.markerFile.create(
    Ci.nsIFile.NORMAL_FILE_TYPE,
    parseInt("0644", 8)
  );
}

/**
 * This test verifies the initialization, indexedDB.open(), and
 * indexedDB.deleteDatabase() aren't blocked when there are unexpected files
 * which haven't been deleted due to some reasons. Normally, we expect every
 * delete operation works fine. Hoever, it's reported that there is only a
 * directory without a corresponding database. It's probably because the delete
 * operation fails for some reasons. P1 introduces the mark-file to let the
 * future operation understand whether there might be unexpected files in idb
 * directory. And, this test verifies these three things work fine if a
 * marker-file, a databse file, and a directory exist in current idb directory.
 */

/* exported testSteps */
async function testSteps() {
  SpecialPowers.setBoolPref("dom.quotaManager.testing", true);

  const name = this.window ? window.location.pathname : "Splendid Test";

  info("Verifying initialization");

  let request = initStorage();
  await requestFinished(request);

  createTestingEnvironment();

  request = initPersistentOrigin(getSystemPrincipal());
  await requestFinished(request);

  let testingFiles = getTestingFiles();
  ok(!testingFiles.dbFile.exists(), "The obsolete database file doesn't exist");
  ok(!testingFiles.dir.exists(), "The obsolete directory doesn't exist");
  ok(!testingFiles.markerFile.exists(), "The marker file doesn't exist");

  info("Verifying open shouldn't be blocked by unexpected files");

  createTestingEnvironment();

  request = indexedDB.open(name);
  await expectingUpgrade(request);
  let event = await expectingSuccess(request);
  ok(true, "The database was opened successfully");
  let db = event.target.result;
  db.close();

  info("Verifying deleteDatabase isn't blocked by unexpected files");

  createTestingEnvironment(true);

  request = indexedDB.deleteDatabase(name);
  await expectingSuccess(request);
  ok(true, "The database was deleted successfully");
}
