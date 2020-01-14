/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify UpgradeStorageFrom2_2To2_3 method.
 */

async function testSteps() {
  function verifyDatabaseTable(shouldExist) {
    let file = getRelativeFile("storage.sqlite");
    let conn = Services.storage.openUnsharedDatabase(file);

    let exists = conn.tableExists("database");
    if (shouldExist) {
      ok(exists, "Database table does exist");
    } else {
      ok(!exists, "Database table does not exist");
    }

    conn.close();
  }

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  // Storage used by FF 68-69 (storage version 2.2).
  installPackage("version2_2_profile");

  verifyDatabaseTable(/* shouldExist */ false);

  info("Initializing");

  // Initialize to trigger storage upgrade from version 2.2
  request = init();
  await requestFinished(request);

  request = reset();
  await requestFinished(request);

  verifyDatabaseTable(/* shouldExist */ true);
}
