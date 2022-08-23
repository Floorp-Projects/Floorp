/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
  const principal = getPrincipal("http://example.com");

  const dataFile = getRelativeFile(
    "storage/default/http+++example.com/ls/data.sqlite"
  );

  const usageJournalFile = getRelativeFile(
    "storage/default/http+++example.com/ls/usage-journal"
  );

  const usageFile = getRelativeFile(
    "storage/default/http+++example.com/ls/usage"
  );

  const data = {};
  data.key = "foo";
  data.value = "bar";
  data.usage = data.key.length + data.value.length;

  async function createStorageForMigration(createUsageDir) {
    info("Clearing");

    let request = clear();
    await requestFinished(request);

    info("Installing package");

    // The profile contains storage.sqlite and webappsstore.sqlite. The file
    // create_db.js in the package was run locally, specifically it was
    // temporarily added to xpcshell.ini and then executed:
    // mach xpcshell-test --interactive dom/localstorage/test/unit/create_db.js
    installPackage("usageAfterMigration_profile");

    if (createUsageDir) {
      // Origin must be initialized before the usage dir is created.

      info("Initializing storage");

      request = initStorage();
      await requestFinished(request);

      info("Initializing temporary storage");

      request = initTemporaryStorage();
      await requestFinished(request);

      info("Initializing origin");

      request = initTemporaryOrigin("default", principal);
      await requestFinished(request);

      info("Creating usage as a directory");

      // This will cause a failure during migration.
      usageFile.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
    }
  }

  function verifyData() {
    ok(dataFile.exists(), "Data file does exist");
  }

  async function verifyUsage(success) {
    info("Verifying usage in memory");

    let request = getOriginUsage(principal, /* fromMemory */ true);
    await requestFinished(request);

    if (success) {
      is(request.result.usage, data.usage, "Correct usage");
    } else {
      is(request.result.usage, 0, "Zero usage");
    }

    info("Verifying usage on disk");

    if (success) {
      ok(!usageJournalFile.exists(), "Usage journal file doesn't exist");
      ok(usageFile.exists(), "Usage file does exist");
      let usage = await readUsageFromUsageFile(usageFile);
      is(usage, data.usage, "Correct usage");
    } else {
      ok(usageJournalFile.exists(), "Usage journal file does exist");
      ok(usageFile.exists(), "Usage file does exist");
    }
  }

  info("Setting prefs");

  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );

  info("Stage 1 - Testing usage after successful data migration");

  await createStorageForMigration(/* createUsageDir */ false);

  info("Getting storage");

  let storage = getLocalStorage(principal);

  info("Opening");

  storage.open();

  verifyData();

  await verifyUsage(/* success */ true);

  info("Stage 2 - Testing usage after unsuccessful data migration");

  await createStorageForMigration(/* createUsageDir */ true);

  info("Getting storage");

  storage = getLocalStorage(principal);

  info("Opening");

  try {
    storage.open();
    ok(false, "Should have thrown");
  } catch (ex) {
    ok(true, "Did throw");
  }

  verifyData();

  await verifyUsage(/* success */ false);

  info("Stage 3 - Testing usage after unsuccessful/successful data migration");

  await createStorageForMigration(/* createUsageDir */ true);

  info("Getting storage");

  storage = getLocalStorage(principal);

  info("Opening");

  try {
    storage.open();
    ok(false, "Should have thrown");
  } catch (ex) {
    ok(true, "Did throw");
  }

  usageFile.remove(true);

  info("Opening");

  storage.open();

  verifyData();

  await verifyUsage(/* success */ true);
});
