/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function getTestingFiles() {
  const filenameBase = "unexpectedDirectory";
  let baseDir = getRelativeFile("storage/permanent/chrome/idb");

  let unexpectedDirWithoutSuffix = baseDir.clone();
  unexpectedDirWithoutSuffix.append(filenameBase);

  let unexpectedDir = baseDir.clone();
  unexpectedDir.append(filenameBase + ".files");

  return { unexpectedDirWithoutSuffix, unexpectedDir };
}

function createTestingEnvironment() {
  let testingFiles = getTestingFiles();
  testingFiles.unexpectedDir.create(
    Ci.nsIFile.DIRECTORY_TYPE,
    parseInt("0755", 8)
  );

  testingFiles.unexpectedDirWithoutSuffix.create(
    Ci.nsIFile.DIRECTORY_TYPE,
    parseInt("0755", 8)
  );
}

/**
 * This test verifies unexpected directories won't block idb's initialization.
 */

/* exported testSteps */
async function testSteps() {
  info("Verifying open shouldn't be blocked by unexpected files");

  createTestingEnvironment();

  let request = indexedDB.open(
    this.window ? window.location.pathname : "Splendid Test",
    1
  );
  await expectingUpgrade(request);

  // Waiting for a success event for indexedDB.open()
  let event = await expectingSuccess(request);

  let testingFiles = getTestingFiles();
  ok(
    !testingFiles.unexpectedDir.exists(),
    "The unexpected directory doesn't exist"
  );
  ok(
    !testingFiles.unexpectedDirWithoutSuffix.exists(),
    "The unexpected directory without the suffix doesn't exist"
  );

  let db = event.target.result;
  db.close();
}
