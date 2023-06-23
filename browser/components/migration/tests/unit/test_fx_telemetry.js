/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const { FirefoxProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/FirefoxProfileMigrator.sys.mjs"
);
const { InternalTestingProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/InternalTestingProfileMigrator.sys.mjs"
);
const { LoginCSVImport } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginCSVImport.sys.mjs"
);
const { PasswordFileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/FileMigrators.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// These preferences are set to true anytime MigratorBase.migrate
// successfully completes a migration of their type.
const BOOKMARKS_PREF = "browser.migrate.interactions.bookmarks";
const CSV_PASSWORDS_PREF = "browser.migrate.interactions.csvpasswords";
const HISTORY_PREF = "browser.migrate.interactions.history";
const PASSWORDS_PREF = "browser.migrate.interactions.passwords";

function readFile(file) {
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  stream.init(file, -1, -1, Ci.nsIFileInputStream.CLOSE_ON_EOF);

  let sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  sis.init(stream);
  let contents = sis.read(file.fileSize);
  sis.close();
  return contents;
}

function checkDirectoryContains(dir, files) {
  print("checking " + dir.path + " - should contain " + Object.keys(files));
  let seen = new Set();
  for (let file of dir.directoryEntries) {
    print("found file: " + file.path);
    Assert.ok(file.leafName in files, file.leafName + " exists, but shouldn't");

    let expectedContents = files[file.leafName];
    if (typeof expectedContents != "string") {
      // it's a subdir - recurse!
      Assert.ok(file.isDirectory(), "should be a subdir");
      let newDir = dir.clone();
      newDir.append(file.leafName);
      checkDirectoryContains(newDir, expectedContents);
    } else {
      Assert.ok(!file.isDirectory(), "should be a regular file");
      let contents = readFile(file);
      Assert.equal(contents, expectedContents);
    }
    seen.add(file.leafName);
  }
  let missing = [];
  for (let x in files) {
    if (!seen.has(x)) {
      missing.push(x);
    }
  }
  Assert.deepEqual(missing, [], "no missing files in " + dir.path);
}

function getTestDirs() {
  // we make a directory structure in a temp dir which mirrors what we are
  // testing.
  let tempDir = do_get_tempdir();
  let srcDir = tempDir.clone();
  srcDir.append("test_source_dir");
  srcDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  let targetDir = tempDir.clone();
  targetDir.append("test_target_dir");
  targetDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  // no need to cleanup these dirs - the xpcshell harness will do it for us.
  return [srcDir, targetDir];
}

function writeToFile(dir, leafName, contents) {
  let file = dir.clone();
  file.append(leafName);

  let outputStream = FileUtils.openFileOutputStream(file);
  outputStream.write(contents, contents.length);
  outputStream.close();
}

function createSubDir(dir, subDirName) {
  let subDir = dir.clone();
  subDir.append(subDirName);
  subDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  return subDir;
}

async function promiseMigrator(name, srcDir, targetDir) {
  // As the FirefoxProfileMigrator is a startup-only migrator, we import its
  // module and instantiate it directly rather than going through MigrationUtils,
  // to bypass that availability check.
  let migrator = new FirefoxProfileMigrator();
  let migrators = migrator._getResourcesInternal(srcDir, targetDir);
  for (let m of migrators) {
    if (m.name == name) {
      return new Promise(resolve => m.migrate(resolve));
    }
  }
  throw new Error("failed to find the " + name + " migrator");
}

function promiseTelemetryMigrator(srcDir, targetDir) {
  return promiseMigrator("telemetry", srcDir, targetDir);
}

add_task(async function test_empty() {
  let [srcDir, targetDir] = getTestDirs();
  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true with empty directories");
  // check both are empty
  checkDirectoryContains(srcDir, {});
  checkDirectoryContains(targetDir, {});
});

add_task(async function test_migrate_files() {
  let [srcDir, targetDir] = getTestDirs();

  // Set up datareporting files, some to copy, some not.
  let stateContent = JSON.stringify({
    clientId: "68d5474e-19dc-45c1-8e9a-81fca592707c",
  });
  let sessionStateContent = "foobar 5432";
  let subDir = createSubDir(srcDir, "datareporting");
  writeToFile(subDir, "state.json", stateContent);
  writeToFile(subDir, "session-state.json", sessionStateContent);
  writeToFile(subDir, "other.file", "do not copy");

  let archived = createSubDir(subDir, "archived");
  writeToFile(archived, "other.file", "do not copy");

  // Set up FHR files, they should not be copied.
  writeToFile(srcDir, "healthreport.sqlite", "do not copy");
  writeToFile(srcDir, "healthreport.sqlite-wal", "do not copy");
  subDir = createSubDir(srcDir, "healthreport");
  writeToFile(subDir, "state.json", "do not copy");
  writeToFile(subDir, "other.file", "do not copy");

  // Perform migration.
  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(
    ok,
    "callback should have been true with important telemetry files copied"
  );

  checkDirectoryContains(targetDir, {
    datareporting: {
      "state.json": stateContent,
      "session-state.json": sessionStateContent,
    },
  });
});

add_task(async function test_datareporting_not_dir() {
  let [srcDir, targetDir] = getTestDirs();

  writeToFile(srcDir, "datareporting", "I'm a file but should be a directory");

  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(
    ok,
    "callback should have been true even though the directory was a file"
  );

  checkDirectoryContains(targetDir, {});
});

add_task(async function test_datareporting_empty() {
  let [srcDir, targetDir] = getTestDirs();

  // Migrate with an empty 'datareporting' subdir.
  createSubDir(srcDir, "datareporting");
  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  // We should end up with no migrated files.
  checkDirectoryContains(targetDir, {
    datareporting: {},
  });
});

add_task(async function test_healthreport_empty() {
  let [srcDir, targetDir] = getTestDirs();

  // Migrate with no 'datareporting' and an empty 'healthreport' subdir.
  createSubDir(srcDir, "healthreport");
  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  // We should end up with no migrated files.
  checkDirectoryContains(targetDir, {});
});

add_task(async function test_datareporting_many() {
  let [srcDir, targetDir] = getTestDirs();

  // Create some datareporting files.
  let subDir = createSubDir(srcDir, "datareporting");
  let shouldBeCopied = "should be copied";
  writeToFile(subDir, "state.json", shouldBeCopied);
  writeToFile(subDir, "session-state.json", shouldBeCopied);
  writeToFile(subDir, "something.else", "should not");
  createSubDir(subDir, "emptyDir");

  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  checkDirectoryContains(targetDir, {
    datareporting: {
      "state.json": shouldBeCopied,
      "session-state.json": shouldBeCopied,
    },
  });
});

add_task(async function test_no_session_state() {
  let [srcDir, targetDir] = getTestDirs();

  // Check that migration still works properly if we only have state.json.
  let subDir = createSubDir(srcDir, "datareporting");
  let stateContent = "abcd984";
  writeToFile(subDir, "state.json", stateContent);

  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  checkDirectoryContains(targetDir, {
    datareporting: {
      "state.json": stateContent,
    },
  });
});

add_task(async function test_no_state() {
  let [srcDir, targetDir] = getTestDirs();

  // Check that migration still works properly if we only have session-state.json.
  let subDir = createSubDir(srcDir, "datareporting");
  let sessionStateContent = "abcd512";
  writeToFile(subDir, "session-state.json", sessionStateContent);

  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  checkDirectoryContains(targetDir, {
    datareporting: {
      "session-state.json": sessionStateContent,
    },
  });
});

add_task(async function test_times_migration() {
  let [srcDir, targetDir] = getTestDirs();

  // create a times.json in the source directory.
  let contents = JSON.stringify({ created: 1234 });
  writeToFile(srcDir, "times.json", contents);

  let earliest = Date.now();
  let ok = await promiseMigrator("times", srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");
  let latest = Date.now();

  let timesFile = targetDir.clone();
  timesFile.append("times.json");

  let raw = readFile(timesFile);
  let times = JSON.parse(raw);
  Assert.ok(times.reset >= earliest && times.reset <= latest);
  // and it should have left the creation time alone.
  Assert.equal(times.created, 1234);
});

/**
 * Tests that when importing bookmarks, history, or passwords, we
 * set interaction prefs. These preferences are sent using
 * TelemetryEnvironment.sys.mjs.
 */
add_task(async function test_interaction_telemetry() {
  let testingMigrator = await MigrationUtils.getMigrator(
    InternalTestingProfileMigrator.key
  );

  Services.prefs.clearUserPref(BOOKMARKS_PREF);
  Services.prefs.clearUserPref(HISTORY_PREF);
  Services.prefs.clearUserPref(PASSWORDS_PREF);

  // Ensure that these prefs start false.
  Assert.ok(!Services.prefs.getBoolPref(BOOKMARKS_PREF));
  Assert.ok(!Services.prefs.getBoolPref(HISTORY_PREF));
  Assert.ok(!Services.prefs.getBoolPref(PASSWORDS_PREF));

  await testingMigrator.migrate(
    MigrationUtils.resourceTypes.BOOKMARKS,
    false,
    InternalTestingProfileMigrator.testProfile
  );
  Assert.ok(
    Services.prefs.getBoolPref(BOOKMARKS_PREF),
    "Bookmarks pref should have been set."
  );
  Assert.ok(!Services.prefs.getBoolPref(HISTORY_PREF));
  Assert.ok(!Services.prefs.getBoolPref(PASSWORDS_PREF));

  await testingMigrator.migrate(
    MigrationUtils.resourceTypes.HISTORY,
    false,
    InternalTestingProfileMigrator.testProfile
  );
  Assert.ok(
    Services.prefs.getBoolPref(BOOKMARKS_PREF),
    "Bookmarks pref should have been set."
  );
  Assert.ok(
    Services.prefs.getBoolPref(HISTORY_PREF),
    "History pref should have been set."
  );
  Assert.ok(!Services.prefs.getBoolPref(PASSWORDS_PREF));

  await testingMigrator.migrate(
    MigrationUtils.resourceTypes.PASSWORDS,
    false,
    InternalTestingProfileMigrator.testProfile
  );
  Assert.ok(
    Services.prefs.getBoolPref(BOOKMARKS_PREF),
    "Bookmarks pref should have been set."
  );
  Assert.ok(
    Services.prefs.getBoolPref(HISTORY_PREF),
    "History pref should have been set."
  );
  Assert.ok(
    Services.prefs.getBoolPref(PASSWORDS_PREF),
    "Passwords pref should have been set."
  );

  // Now make sure that we still record these if we migrate a
  // series of resources at the same time.
  Services.prefs.clearUserPref(BOOKMARKS_PREF);
  Services.prefs.clearUserPref(HISTORY_PREF);
  Services.prefs.clearUserPref(PASSWORDS_PREF);

  await testingMigrator.migrate(
    MigrationUtils.resourceTypes.ALL,
    false,
    InternalTestingProfileMigrator.testProfile
  );
  Assert.ok(
    Services.prefs.getBoolPref(BOOKMARKS_PREF),
    "Bookmarks pref should have been set."
  );
  Assert.ok(
    Services.prefs.getBoolPref(HISTORY_PREF),
    "History pref should have been set."
  );
  Assert.ok(
    Services.prefs.getBoolPref(PASSWORDS_PREF),
    "Passwords pref should have been set."
  );
});

/**
 * Tests that when importing passwords from a CSV file using the
 * migration wizard, we set an interaction pref. This preference
 * is sent using TelemetryEnvironment.sys.mjs.
 */
add_task(async function test_csv_password_interaction_telemetry() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  let testingMigrator = new PasswordFileMigrator();

  Services.prefs.clearUserPref(CSV_PASSWORDS_PREF);
  Assert.ok(!Services.prefs.getBoolPref(CSV_PASSWORDS_PREF));

  sandbox.stub(LoginCSVImport, "importFromCSV").resolves([]);
  await testingMigrator.migrate("some/fake/path.csv");

  Assert.ok(
    Services.prefs.getBoolPref(CSV_PASSWORDS_PREF),
    "CSV import pref should have been set."
  );

  sandbox.restore();
});

/**
 * Tests that interaction preferences used for TelemetryEnvironment are
 * persisted across profile resets.
 */
add_task(async function test_interaction_telemetry_persist_across_reset() {
  const PREFS = `
user_pref("${BOOKMARKS_PREF}", true);
user_pref("${CSV_PASSWORDS_PREF}", true);
user_pref("${HISTORY_PREF}", true);
user_pref("${PASSWORDS_PREF}", true);
  `;

  let [srcDir, targetDir] = getTestDirs();
  writeToFile(srcDir, "prefs.js", PREFS);

  let ok = await promiseTelemetryMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  let prefsPath = PathUtils.join(targetDir.path, "prefs.js");
  Assert.ok(await IOUtils.exists(prefsPath), "Prefs should have been written.");
  let writtenPrefsString = await IOUtils.readUTF8(prefsPath);
  for (let prefKey of [
    BOOKMARKS_PREF,
    CSV_PASSWORDS_PREF,
    HISTORY_PREF,
    PASSWORDS_PREF,
  ]) {
    const EXPECTED = `user_pref("${prefKey}", true);`;
    Assert.ok(writtenPrefsString.includes(EXPECTED), "Found persisted pref.");
  }
});
