/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * The goal of this test is to prove that orphaned files are cleaned up during
 * origin initialization. A file is orphaned when there's a file with zero size
 * in the $dbName.files/journals directory and the file table in the database
 * contains no records for given id. A file can become orphaned when we didn't
 * have a chance to remove the file from disk during shutdown or the app just
 * crashed.
 */

/* exported testSteps */
async function testSteps() {
  const name = "test_orphaned_files.js";

  const objectStoreName = "Blobs";

  const blobData = { key: 1 };

  info("Installing profile");

  let request = clearAllDatabases();
  await requestFinished(request);

  // The profile contains one initialized origin directory (with an IndexedDB
  // database and an orphaned file), a script for origin initialization and the
  // storage database:
  // - storage/permanent/chrome
  // - create_db.js
  // - storage.sqlite
  // The file create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/indexedDB/test/unit/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Remove the file "storage/ls-archive.sqlite".

  installPackagedProfile("orphaned_files_profile");

  info("Opening database");

  request = indexedDB.open(name);
  await expectingSuccess(request);

  info("Getting data");

  request = request.result
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(blobData.key);
  await requestSucceeded(request);

  info("Verifying data");

  ok(request.result === undefined, "Correct result");
}
