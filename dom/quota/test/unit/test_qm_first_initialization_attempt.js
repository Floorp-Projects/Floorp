/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const telemetry = "QM_FIRST_INITIALIZATION_ATTEMPT";

const testcases = [
  {
    key: "Storage",
    initFunction: init,
    initArgs: [[]],
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    async breakInit() {
      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    unbreakInit() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "TemporaryStorage",
    initFunction: initTemporaryStorage,
    initArgs: [[]],
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    async breakInit() {
      // We need to initialize storage before creating the metadata directory.
      // If we don't do that, the storage directory created for the metadata
      // directory would trigger storage upgrades (from version 0 to current
      // version) which would fail due to the metadata directory entry being
      // a directory (not a file).
      let request = init();
      await requestFinished(request);

      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    unbreakInit() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "PersistentOrigin",
    initFunction: initStorageAndOrigin,
    initArgs: [
      [getPrincipal("https://example.com"), "persistent"],
      [getPrincipal("https://example1.com"), "persistent"],
      [getPrincipal("https://example2.com"), "default"],
    ],
    get originFiles() {
      return [
        getRelativeFile("storage/permanent/https+++example.com"),
        getRelativeFile("storage/permanent/https+++example1.com"),
        getRelativeFile("storage/default/https+++example2.com"),
      ];
    },
    async breakInit() {
      // We need to initialize temporary storage before creating the origin
      // file. If we don't do that, the initialization would fail while
      // initializating the temporary storage.
      let request = initTemporaryStorage();
      await requestFinished(request);

      for (let originFile of this.originFiles) {
        originFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
      }
    },
    unbreakInit() {
      for (let originFile of this.originFiles) {
        originFile.remove(false);
      }
    },
    expectedResult: {
      // Only the first init results for (persistent, example1.com) and
      // (persistent, example2.com) should be reported.
      initFailure: [2, 0],
      initFailureThenSuccess: [2, 2, 0],
    },
  },
  {
    key: "TemporaryOrigin",
    initFunction: initStorageAndOrigin,
    initArgs: [
      [getPrincipal("https://example.com"), "temporary"],
      [getPrincipal("https://example.com"), "default"],
      [getPrincipal("https://example1.com"), "default"],
      [getPrincipal("https://example2.com"), "persistent"],
    ],
    get originFiles() {
      return [
        getRelativeFile("storage/temporary/https+++example.com"),
        getRelativeFile("storage/default/https+++example.com"),
        getRelativeFile("storage/default/https+++example1.com"),
        getRelativeFile("storage/permanent/https+++example2.com"),
      ];
    },
    async breakInit() {
      // We need to initialize temporary storage before creating the origin
      // file. If we don't do that, the initialization would fail while
      // initializating the temporary storage.
      let request = initTemporaryStorage();
      await requestFinished(request);

      for (let originFile of this.originFiles) {
        originFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
      }
    },
    unbreakInit() {
      for (let originFile of this.originFiles) {
        originFile.remove(false);
      }
    },
    expectedResult: {
      // Only the first result of EnsureTemporaryOriginIsInitialized per origin
      // should be reported. Thus, only the results for
      // (temporary, example.com), and (default, example1.com) should be
      // reported.
      initFailure: [2, 0],
      initFailureThenSuccess: [2, 2, 0],
    },
  },
  {
    // Tests for upgrade functions are sligthy different from others because
    // it's impossible to run the testing function in the success cases twice
    // without calling reset().

    key: "UpgradeStorageFrom0_0To1_0",
    initFunction: init,
    initArgs: [[]],
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    initSetting() {
      let db = getRelativeFile("storage.sqlite");
      if (db.exists()) {
        // Remove the storage.sqlite to test the code path for version0_0To1_0
        db.remove(false);
      }
    },
    async breakInit() {
      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    unbreakInit() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "UpgradeStorageFrom1_0To2_0",
    initFunction: init,
    initArgs: [[]],
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    initSetting() {
      // The zipped file contins only one storage:
      // - storage.sqlite
      // To make it become the profile in the test, we need:
      // 1. Get the storage.sqlite from a clean profile.
      // 2. Make the version of it to 1_0 version by
      //    sqlite3 PROFILE/storage.sqlite "PRAGMA user_version = 65536;"
      installPackage("version2_0upgrade_profile");
    },
    async breakInit() {
      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    unbreakInit() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "UpgradeStorageFrom2_0To2_1",
    initFunction: init,
    initArgs: [[]],
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    initSetting() {
      installPackage("version2_1upgrade_profile");
    },
    async breakInit() {
      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    unbreakInit() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "UpgradeStorageFrom2_1To2_2",
    initFunction: init,
    initArgs: [[]],
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    initSetting() {
      installPackage("version2_2upgrade_profile");
    },
    async breakInit() {
      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    unbreakInit() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "UpgradeStorageFrom2_2To2_3",
    initFunction: init,
    initArgs: [[]],
    initSetting() {
      // Reuse the version2_1 storage.sqlite to test the code path for 2_2To2_3
      // upgrade
      installPackage("version2_2upgrade_profile");
    },
    async breakInit() {
      // The zipped file contins only one storage:
      // - storage.sqlite
      // To make it become the profile in the test, we need:
      // 1. Get the storage.sqlite from a clean profile.
      // 2. Make the version of it to 2_2 version by
      //    sqlite3 PROFILE/storage.sqlite "PRAGMA user_version = 131074;"
      // 3. Make it broken by creating the table in the upgrading function by
      //    sqlite3 PROFILE/storage.sqlite "CREATE TABLE database ( \
      //      "cache_version INTEGER NOT NULL DEFAULT 0);"
      installPackage("broken2_2_profile");
    },
    unbreakInit() {
      let db = getRelativeFile("storage.sqlite");
      db.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "DefaultRepository",
    initFunction: initTemporaryStorage,
    initArgs: [[]],
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    async initSetting() {
      // Create a valid file/directtory in the target repository is needed.
      // Otherwise, we can not test the code path.
      let originDir = getRelativeFile("storage/default/https+++example1.com/");

      if (!originDir.exists()) {
        originDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
      }
    },
    async breakInit() {
      let request = init();
      await requestFinished(request);

      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    unbreakInit() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "TemporaryRepository",
    initFunction: initTemporaryStorage,
    initArgs: [[]],
    get metadataDir() {
      return getRelativeFile(
        "storage/temporary/https+++example.com/.metadata-v2"
      );
    },
    async initSetting() {
      let originDir = getRelativeFile(
        "storage/temporary/https+++example1.com/"
      );

      if (!originDir.exists()) {
        originDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
      }
    },
    async breakInit() {
      let request = init();
      await requestFinished(request);

      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    unbreakInit() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
];

function verifyResult(histogram, key, expectedResult) {
  const snapshot = histogram.snapshot();

  ok(key in snapshot, `The histogram ${histogram.name()} must contain ${key}`);

  is(
    Object.entries(snapshot[key].values).length,
    expectedResult.length,
    `Reported telemetry should have the same size as expected result (${
      expectedResult.length
    })`
  );

  for (let i = 0; i < expectedResult.length; ++i) {
    is(
      snapshot[key].values[i],
      expectedResult[i],
      `Expected counts should match for ${histogram.name()} at index ${i}`
    );
  }
}

async function testSteps() {
  let request;
  for (let testcase of testcases) {
    const key = testcase.key;

    info("Verifying the telemetry probe " + telemetry + " for the key " + key);

    const histogram = TelemetryTestUtils.getAndClearKeyedHistogram(telemetry);

    for (let expectedInitResult of [false, true]) {
      info(
        "Verifying the reported result on the Telemetry is expected when " +
          "the init " +
          (expectedInitResult ? "succeeds" : "fails")
      );

      if (testcase.initSetting) {
        await testcase.initSetting();
      }

      if (!expectedInitResult) {
        await testcase.breakInit();
      }

      const msg =
        "Should " + (expectedInitResult ? "not " : "") + "have thrown";

      // The steps below verify we should get the result of the first attempt
      // for the initialization.
      for (let i = 0; i < 2; ++i) {
        for (let initArg of testcase.initArgs) {
          request = testcase.initFunction(...initArg);
          try {
            await requestFinished(request);
            ok(expectedInitResult, msg);
          } catch (ex) {
            ok(!expectedInitResult, msg);
          }
        }
      }

      if (!expectedInitResult) {
        // Remove the setting (e.g. files) created by the breakInit(). We need
        // to do that for the next iteration in which the initialization is
        // supposed to succeed.
        testcase.unbreakInit();
      }

      const expectedResultForThisRun = expectedInitResult
        ? testcase.expectedResult.initFailureThenSuccess
        : testcase.expectedResult.initFailure;
      verifyResult(histogram, key, expectedResultForThisRun);

      // The first initialization attempt has been reported in telemetry and
      // any new attemps wouldn't be reported if we don't reset the storage
      // here.
      request = reset();
      await requestFinished(request);
    }

    request = clear();
    await requestFinished(request);
  }
}
