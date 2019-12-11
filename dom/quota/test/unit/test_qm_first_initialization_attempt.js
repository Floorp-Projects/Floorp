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
    testingInitFunction() {
      return init();
    },
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    async settingForForcingInitFailure() {
      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    removeSetting() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "TemporaryStorage",
    testingInitFunction() {
      return initTemporaryStorage();
    },
    get metadataDir() {
      return getRelativeFile(
        "storage/default/https+++example.com/.metadata-v2"
      );
    },
    async settingForForcingInitFailure() {
      // We need to initialize storage before creating the metadata directory.
      // If we don't do that, the storage directory created for the metadata
      // directory would trigger storage upgrades (from version 0 to current
      // version) which would fail due to the metadata directory entry being
      // a directory (not a file).
      let request = init();
      await requestFinished(request);

      this.metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    },
    removeSetting() {
      this.metadataDir.remove(false);
    },
    expectedResult: {
      initFailure: [1, 0],
      initFailureThenSuccess: [1, 1, 0],
    },
  },
  {
    key: "PersistentOrigin",
    testingInitFunction: [
      function() {
        return initStorageAndOrigin(
          getPrincipal("https://example1.com"),
          "persistent"
        );
      },
      function() {
        return initStorageAndOrigin(
          getPrincipal("https://example2.com"),
          "persistent"
        );
      },
      function() {
        return initStorageAndOrigin(
          getPrincipal("https://example3.com"),
          "default"
        );
      },
    ],
    get originFiles() {
      return [
        getRelativeFile("storage/permanent/https+++example1.com"),
        getRelativeFile("storage/permanent/https+++example2.com"),
        getRelativeFile("storage/default/https+++example3.com"),
      ];
    },
    async settingForForcingInitFailure() {
      // We need to initialize temporary storage before creating the origin
      // file. If we don't do that, the initialization would fail while
      // initializating the temporary storage.
      let request = initTemporaryStorage();
      await requestFinished(request);

      for (let originFile of this.originFiles) {
        originFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
      }
    },
    removeSetting() {
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
    testingInitFunction: [
      function() {
        return initStorageAndOrigin(
          getPrincipal("https://example.com"),
          "temporary"
        );
      },
      function() {
        return initStorageAndOrigin(
          getPrincipal("https://example.com"),
          "default"
        );
      },
      function() {
        return initStorageAndOrigin(
          getPrincipal("https://example1.com"),
          "default"
        );
      },
      function() {
        return initStorageAndOrigin(
          getPrincipal("https://example2.com"),
          "persistent"
        );
      },
    ],
    get originFiles() {
      return [
        getRelativeFile("storage/temporary/https+++example.com"),
        getRelativeFile("storage/default/https+++example.com"),
        getRelativeFile("storage/default/https+++example1.com"),
        getRelativeFile("storage/permanent/https+++example2.com"),
      ];
    },
    async settingForForcingInitFailure() {
      // We need to initialize temporary storage before creating the origin
      // file. If we don't do that, the initialization would fail while
      // initializating the temporary storage.
      let request = initTemporaryStorage();
      await requestFinished(request);

      for (let originFile of this.originFiles) {
        originFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
      }
    },
    removeSetting() {
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

      if (!expectedInitResult) {
        await testcase.settingForForcingInitFailure();
      }

      const msg =
        "Should " + (expectedInitResult ? "not " : "") + "have thrown";

      // The steps below verify we should get the result of the first attempt
      // for the initialization.
      for (let i = 0; i < 2; ++i) {
        const iterableInitFunc =
          typeof testcase.testingInitFunction[Symbol.iterator] === "function"
            ? testcase.testingInitFunction
            : [testcase.testingInitFunction];
        for (let initFunc of iterableInitFunc) {
          request = initFunc();
          try {
            await requestFinished(request);
            ok(expectedInitResult, msg);
          } catch (ex) {
            ok(!expectedInitResult, msg);
          }
        }
      }

      if (!expectedInitResult) {
        // Remove the setting (e.g. files) created by the
        // settingForForcingInitFilaure(). We need to do that for the next
        // iteration in which the initialization is supposed to succeed.
        testcase.removeSetting();
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
