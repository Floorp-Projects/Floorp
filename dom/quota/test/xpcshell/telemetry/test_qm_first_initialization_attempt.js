/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AppConstants } = Cu.import("resource://gre/modules/AppConstants.jsm");
const { TelemetryTestUtils } = Cu.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const storageDirName = "storage";
const storageFileName = "storage.sqlite";
const indexedDBDirName = "indexedDB";
const persistentStorageDirName = "storage/persistent";
const histogramName = "QM_FIRST_INITIALIZATION_ATTEMPT";

const testcases = [
  {
    mainKey: "Storage",
    async setup(expectedInitResult) {
      if (!expectedInitResult) {
        // Make the database unusable by creating it as a directory (not a
        // file).
        const storageFile = getRelativeFile(storageFileName);
        storageFile.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
      }
    },
    initFunction: init,
    expectedSnapshots: {
      initFailure: {
        // mainKey
        Storage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        // mainKey
        Storage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "TemporaryStorage",
    async setup(expectedInitResult) {
      // We need to initialize storage before populating the repositories. If
      // we don't do that, the storage directory created for the repositories
      // would trigger storage upgrades (from version 0 to current version).
      let request = init();
      await requestFinished(request);

      populateRepository("temporary");
      populateRepository("default");

      if (!expectedInitResult) {
        makeRepositoryUnusable("temporary");
        makeRepositoryUnusable("default");
      }
    },
    initFunction: initTemporaryStorage,
    getExpectedSnapshots() {
      const expectedSnapshotsInNightly = {
        initFailure: {
          Storage: {
            values: [0, 1, 0],
          },
          TemporaryRepository: {
            values: [1, 0],
          },
          DefaultRepository: {
            values: [1, 0],
          },
          // mainKey
          TemporaryStorage: {
            values: [1, 0],
          },
        },
        initFailureThenSuccess: {
          Storage: {
            values: [0, 2, 0],
          },
          TemporaryRepository: {
            values: [1, 1, 0],
          },
          DefaultRepository: {
            values: [1, 1, 0],
          },
          // mainKey
          TemporaryStorage: {
            values: [1, 1, 0],
          },
        },
      };

      const expectedSnapshotsInOthers = {
        initFailure: {
          Storage: {
            values: [0, 1, 0],
          },
          TemporaryRepository: {
            values: [1, 0],
          },
          // mainKey
          TemporaryStorage: {
            values: [1, 0],
          },
        },
        initFailureThenSuccess: {
          Storage: {
            values: [0, 2, 0],
          },
          TemporaryRepository: {
            values: [1, 1, 0],
          },
          DefaultRepository: {
            values: [0, 1, 0],
          },
          // mainKey
          TemporaryStorage: {
            values: [1, 1, 0],
          },
        },
      };

      return AppConstants.NIGHTLY_BUILD
        ? expectedSnapshotsInNightly
        : expectedSnapshotsInOthers;
    },
  },
  {
    mainKey: "DefaultRepository",
    async setup(expectedInitResult) {
      // See the comment for "TemporaryStorage".
      let request = init();
      await requestFinished(request);

      populateRepository("default");

      if (!expectedInitResult) {
        makeRepositoryUnusable("default");
      }
    },
    initFunction: initTemporaryStorage,
    expectedSnapshots: {
      initFailure: {
        Storage: {
          values: [0, 1, 0],
        },
        TemporaryRepository: {
          values: [0, 1, 0],
        },
        // mainKey
        DefaultRepository: {
          values: [1, 0],
        },
        TemporaryStorage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        Storage: {
          values: [0, 2, 0],
        },
        TemporaryRepository: {
          values: [0, 2, 0],
        },
        // mainKey
        DefaultRepository: {
          values: [1, 1, 0],
        },
        TemporaryStorage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "TemporaryRepository",
    async setup(expectedInitResult) {
      // See the comment for "TemporaryStorage".
      let request = init();
      await requestFinished(request);

      populateRepository("temporary");

      if (!expectedInitResult) {
        makeRepositoryUnusable("temporary");
      }
    },
    initFunction: initTemporaryStorage,
    getExpectedSnapshots() {
      const expectedSnapshotsInNightly = {
        initFailure: {
          Storage: {
            values: [0, 1, 0],
          },
          // mainKey
          TemporaryRepository: {
            values: [1, 0],
          },
          DefaultRepository: {
            values: [0, 1, 0],
          },
          TemporaryStorage: {
            values: [1, 0],
          },
        },
        initFailureThenSuccess: {
          Storage: {
            values: [0, 2, 0],
          },
          // mainKey
          TemporaryRepository: {
            values: [1, 1, 0],
          },
          DefaultRepository: {
            values: [0, 2, 0],
          },
          TemporaryStorage: {
            values: [1, 1, 0],
          },
        },
      };

      const expectedSnapshotsInOthers = {
        initFailure: {
          Storage: {
            values: [0, 1, 0],
          },
          // mainKey
          TemporaryRepository: {
            values: [1, 0],
          },
          TemporaryStorage: {
            values: [1, 0],
          },
        },
        initFailureThenSuccess: {
          Storage: {
            values: [0, 2, 0],
          },
          // mainKey
          TemporaryRepository: {
            values: [1, 1, 0],
          },
          DefaultRepository: {
            values: [0, 1, 0],
          },
          TemporaryStorage: {
            values: [1, 1, 0],
          },
        },
      };

      return AppConstants.NIGHTLY_BUILD
        ? expectedSnapshotsInNightly
        : expectedSnapshotsInOthers;
    },
  },
  {
    mainKey: "UpgradeStorageFrom0_0To1_0",
    async setup(expectedInitResult) {
      // storage used prior FF 49 (storage version 0.0)
      installPackage("version0_0_profile");

      if (!expectedInitResult) {
        installPackage("version0_0_make_it_unusable");
      }
    },
    initFunction: init,
    expectedSnapshots: {
      initFailure: {
        // mainKey
        UpgradeStorageFrom0_0To1_0: {
          values: [1, 0],
        },
        Storage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        // mainKey
        UpgradeStorageFrom0_0To1_0: {
          values: [1, 1, 0],
        },
        UpgradeStorageFrom1_0To2_0: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_0To2_1: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_1To2_2: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_2To2_3: {
          values: [0, 1, 0],
        },
        Storage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "UpgradeStorageFrom1_0To2_0",
    async setup(expectedInitResult) {
      // storage used by FF 49-54 (storage version 1.0)
      installPackage("version1_0_profile");

      if (!expectedInitResult) {
        installPackage("version1_0_make_it_unusable");
      }
    },
    initFunction: init,
    expectedSnapshots: {
      initFailure: {
        // mainKey
        UpgradeStorageFrom1_0To2_0: {
          values: [1, 0],
        },
        Storage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        // mainKey
        UpgradeStorageFrom1_0To2_0: {
          values: [1, 1, 0],
        },
        UpgradeStorageFrom2_0To2_1: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_1To2_2: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_2To2_3: {
          values: [0, 1, 0],
        },
        Storage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "UpgradeStorageFrom2_0To2_1",
    async setup(expectedInitResult) {
      // storage used by FF 55-56 (storage version 2.0)
      installPackage("version2_0_profile");

      if (!expectedInitResult) {
        installPackage("version2_0_make_it_unusable");
      }
    },
    initFunction: init,
    expectedSnapshots: {
      initFailure: {
        // mainKey
        UpgradeStorageFrom2_0To2_1: {
          values: [1, 0],
        },
        Storage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        // mainKey
        UpgradeStorageFrom2_0To2_1: {
          values: [1, 1, 0],
        },
        UpgradeStorageFrom2_1To2_2: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_2To2_3: {
          values: [0, 1, 0],
        },
        Storage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "UpgradeStorageFrom2_1To2_2",
    async setup(expectedInitResult) {
      // storage used by FF 57-67 (storage version 2.1)
      installPackage("version2_1_profile");

      if (!expectedInitResult) {
        installPackage("version2_1_make_it_unusable");
      }
    },
    initFunction: init,
    expectedSnapshots: {
      initFailure: {
        // mainKey
        UpgradeStorageFrom2_1To2_2: {
          values: [1, 0],
        },
        Storage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        // mainKey
        UpgradeStorageFrom2_1To2_2: {
          values: [1, 1, 0],
        },
        UpgradeStorageFrom2_2To2_3: {
          values: [0, 1, 0],
        },
        Storage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "UpgradeStorageFrom2_2To2_3",
    async setup(expectedInitResult) {
      // storage used by FF 68-69 (storage version 2.2)
      installPackage("version2_2_profile");

      if (!expectedInitResult) {
        installPackage(
          "version2_2_make_it_unusable",
          /* allowFileOverwrites */ true
        );
      }
    },
    initFunction: init,
    expectedSnapshots: {
      initFailure: {
        // mainKey
        UpgradeStorageFrom2_2To2_3: {
          values: [1, 0],
        },
        Storage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        // mainKey
        UpgradeStorageFrom2_2To2_3: {
          values: [1, 1, 0],
        },
        Storage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "UpgradeFromIndexedDBDirectory",
    async setup(expectedInitResult) {
      const indexedDBDir = getRelativeFile(indexedDBDirName);
      indexedDBDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

      if (!expectedInitResult) {
        // "indexedDB" directory will be moved under "storage" directory and at
        // the same time renamed to "persistent". Create a storage file to cause
        // the moves to fail.
        const storageFile = getRelativeFile(storageDirName);
        storageFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
      }
    },
    initFunction: init,
    expectedSnapshots: {
      initFailure: {
        // mainKey
        UpgradeFromIndexedDBDirectory: {
          values: [1, 0],
        },
        Storage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        // mainKey
        UpgradeFromIndexedDBDirectory: {
          values: [1, 1, 0],
        },
        UpgradeFromPersistentStorageDirectory: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom0_0To1_0: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom1_0To2_0: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_0To2_1: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_1To2_2: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_2To2_3: {
          values: [0, 1, 0],
        },
        Storage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "UpgradeFromPersistentStorageDirectory",
    async setup(expectedInitResult) {
      const persistentStorageDir = getRelativeFile(persistentStorageDirName);
      persistentStorageDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

      if (!expectedInitResult) {
        // Create a metadata directory to break creating or upgrading directory
        // metadata files.
        const metadataDir = getRelativeFile(
          "storage/persistent/https+++bad.example.com/.metadata"
        );
        metadataDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
      }
    },
    initFunction: init,
    expectedSnapshots: {
      initFailure: {
        // mainKey
        UpgradeFromPersistentStorageDirectory: {
          values: [1, 0],
        },
        Storage: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        // mainKey
        UpgradeFromPersistentStorageDirectory: {
          values: [1, 1, 0],
        },
        UpgradeStorageFrom0_0To1_0: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom1_0To2_0: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_0To2_1: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_1To2_2: {
          values: [0, 1, 0],
        },
        UpgradeStorageFrom2_2To2_3: {
          values: [0, 1, 0],
        },
        Storage: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "PersistentOrigin",
    async setup(expectedInitResult) {
      if (!expectedInitResult) {
        const originFiles = [
          getRelativeFile("storage/permanent/https+++example.com"),
          getRelativeFile("storage/permanent/https+++example1.com"),
          getRelativeFile("storage/default/https+++example2.com"),
        ];

        // We need to initialize storage before creating the origin files. If
        // we don't do that, the storage directory created for the origin files
        // would trigger storage upgrades (from version 0 to current version).
        let request = init();
        await requestFinished(request);

        for (const originFile of originFiles) {
          originFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
        }
      }
    },
    initFunction: initStorageAndOrigin,
    initArgs: [
      [getPrincipal("https://example.com"), "persistent"],
      [getPrincipal("https://example1.com"), "persistent"],
      [getPrincipal("https://example2.com"), "default"],
    ],
    expectedSnapshots: {
      initFailure: {
        Storage: {
          values: [0, 1, 0],
        },
        TemporaryRepository: {
          values: [0, 1, 0],
        },
        DefaultRepository: {
          values: [0, 1, 0],
        },
        TemporaryStorage: {
          values: [0, 1, 0],
        },
        // mainKey
        PersistentOrigin: {
          values: [2, 0],
        },
        TemporaryOrigin: {
          values: [1, 0],
        },
      },
      initFailureThenSuccess: {
        Storage: {
          values: [0, 2, 0],
        },
        TemporaryRepository: {
          values: [0, 2, 0],
        },
        DefaultRepository: {
          values: [0, 2, 0],
        },
        TemporaryStorage: {
          values: [0, 2, 0],
        },
        // mainKey
        PersistentOrigin: {
          values: [2, 2, 0],
        },
        TemporaryOrigin: {
          values: [1, 1, 0],
        },
      },
    },
  },
  {
    mainKey: "TemporaryOrigin",
    async setup(expectedInitResult) {
      if (!expectedInitResult) {
        const originFiles = [
          getRelativeFile("storage/temporary/https+++example.com"),
          getRelativeFile("storage/default/https+++example.com"),
          getRelativeFile("storage/default/https+++example1.com"),
          getRelativeFile("storage/permanent/https+++example2.com"),
        ];

        // See the comment for "PersistentOrigin".
        let request = init();
        await requestFinished(request);

        for (const originFile of originFiles) {
          originFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
        }
      }
    },
    initFunction: initStorageAndOrigin,
    initArgs: [
      [getPrincipal("https://example.com"), "temporary"],
      [getPrincipal("https://example.com"), "default"],
      [getPrincipal("https://example1.com"), "default"],
      [getPrincipal("https://example2.com"), "persistent"],
    ],
    // Only the first result of EnsureTemporaryOriginIsInitialized per origin
    // should be reported. Thus, only the results for (temporary, example.com),
    // and (default, example1.com) should be reported.
    expectedSnapshots: {
      initFailure: {
        Storage: {
          values: [0, 1, 0],
        },
        TemporaryRepository: {
          values: [0, 1, 0],
        },
        DefaultRepository: {
          values: [0, 1, 0],
        },
        TemporaryStorage: {
          values: [0, 1, 0],
        },
        PersistentOrigin: {
          values: [1, 0],
        },
        // mainKey
        TemporaryOrigin: {
          values: [2, 0],
        },
      },
      initFailureThenSuccess: {
        Storage: {
          values: [0, 2, 0],
        },
        TemporaryRepository: {
          values: [0, 2, 0],
        },
        DefaultRepository: {
          values: [0, 2, 0],
        },
        TemporaryStorage: {
          values: [0, 2, 0],
        },
        PersistentOrigin: {
          values: [1, 1, 0],
        },
        // mainKey
        TemporaryOrigin: {
          values: [2, 2, 0],
        },
      },
    },
  },
];

loadScript("dom/quota/test/xpcshell/common/utils.js");

function verifyHistogram(histogram, mainKey, expectedSnapshot) {
  const snapshot = histogram.snapshot();

  ok(
    mainKey in snapshot,
    `The histogram ${histogram.name()} must contain the main key ${mainKey}`
  );

  const keys = Object.keys(snapshot);

  is(
    keys.length,
    Object.keys(expectedSnapshot).length,
    `The number of keys must match the expected number of keys for ` +
      `${histogram.name()}`
  );

  for (const key of keys) {
    ok(
      key in expectedSnapshot,
      `The key ${key} must match the expected keys for ${histogram.name()}`
    );

    const values = Object.entries(snapshot[key].values);
    const expectedValues = expectedSnapshot[key].values;

    is(
      values.length,
      expectedValues.length,
      `The number of values should match the expected number of values for ` +
        `${histogram.name()}`
    );

    for (let [i, val] of values) {
      is(
        val,
        expectedValues[i],
        `Expected counts should match for ${histogram.name()} at index ${i}`
      );
    }
  }
}

async function testSteps() {
  let request;
  for (const testcase of testcases) {
    const mainKey = testcase.mainKey;

    info(`Verifying ${histogramName} histogram for the main key ${mainKey}`);

    const histogram = TelemetryTestUtils.getAndClearKeyedHistogram(
      histogramName
    );

    for (const expectedInitResult of [false, true]) {
      info(
        `Verifying the histogram when the initialization ` +
          `${expectedInitResult ? "failed and then succeeds" : "fails"}`
      );

      await testcase.setup(expectedInitResult);

      const msg = `Should ${expectedInitResult ? "not " : ""} have thrown`;

      // Call the initialization function twice, so we can verify below that
      // only the first initialization attempt has been reported.
      for (let i = 0; i < 2; ++i) {
        const initArgs = testcase.initArgs ? testcase.initArgs : [[]];
        for (const initArg of initArgs) {
          request = testcase.initFunction(...initArg);
          try {
            await requestFinished(request);
            ok(expectedInitResult, msg);
          } catch (ex) {
            ok(!expectedInitResult, msg);
          }
        }
      }

      const expectedSnapshots = testcase.getExpectedSnapshots
        ? testcase.getExpectedSnapshots()
        : testcase.expectedSnapshots;

      const expectedSnapshot = expectedInitResult
        ? expectedSnapshots.initFailureThenSuccess
        : expectedSnapshots.initFailure;

      verifyHistogram(histogram, mainKey, expectedSnapshot);

      // The first initialization attempt has been reported in the histogram
      // and any new attemps wouldn't be reported if we didn't reset or clear
      // the storage here. We need a clean profile for the next iteration
      // anyway.
      // However, the clear storage operation needs initialized storage, so
      // clearing can fail if the storage is unusable and it can also increase
      // some of the telemetry counters. Instead of calling clear, we can just
      // call reset and clear profile manually.
      request = reset();
      await requestFinished(request);

      const indexedDBDir = getRelativeFile(indexedDBDirName);
      if (indexedDBDir.exists()) {
        indexedDBDir.remove(false);
      }

      const storageDir = getRelativeFile(storageDirName);
      if (storageDir.exists()) {
        storageDir.remove(true);
      }

      const storageFile = getRelativeFile(storageFileName);
      if (storageFile.exists()) {
        // It could be a non empty directory, so remove it recursively.
        storageFile.remove(true);
      }
    }
  }
}
