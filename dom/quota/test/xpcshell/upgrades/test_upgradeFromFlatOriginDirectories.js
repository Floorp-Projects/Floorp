/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// A flat origin directory is an origin directory with no sub directories for
// quota clients. The upgrade was initially done lazily and an empty .metadata
// file was used to indicate a successful upgrade.

function* testSteps() {
  const setups = [
    {
      packages: [
        // Storage used prior FF 22 (indexedDB/ directory with flat origin
        // directories).
        // FF 26 renamed indexedDB/ to storage/persistent and the lazy upgrade
        // of flat origin directories remained. There's a test for that below.
        "indexedDBDirectory_flatOriginDirectories_profile",
        "../indexedDBDirectory_shared",
      ],
      origins: [
        {
          oldPath: "indexedDB/1007+f+app+++system.gaiamobile.org",
        },

        {
          oldPath: "indexedDB/1007+t+https+++developer.cdn.mozilla.net",
        },

        {
          oldPath: "indexedDB/http+++www.mozilla.org",
          newPath: "storage/default/http+++www.mozilla.org",
          url: "http://www.mozilla.org",
          persistence: "default",
        },
      ],
    },

    {
      packages: [
        // Storage used by FF 26-35 (storage/persistent/ directory with not yet
        // upgraded flat origin directories).
        // FF 36 renamed storage/persistent/ to storage/default/ and all not
        // yet upgraded flat origin directories were upgraded. There's a
        // separate test for that:
        // test_upgradeFromPersistentStorageDirectory_upgradeOriginDirectories.
        "persistentStorageDirectory_flatOriginDirectories_profile",
        "../persistentStorageDirectory_shared",
      ],
      origins: [
        {
          oldPath: "storage/persistent/1007+f+app+++system.gaiamobile.org",
        },

        {
          oldPath:
            "storage/persistent/1007+t+https+++developer.cdn.mozilla.net",
        },

        {
          oldPath: "storage/persistent/http+++www.mozilla.org",
          newPath: "storage/default/http+++www.mozilla.org",
          url: "http://www.mozilla.org",
          persistence: "default",
        },
      ],
    },
  ];

  const metadataFileName = ".metadata";

  for (const setup of setups) {
    clear(continueToNextStepSync);
    yield undefined;

    info("Verifying storage");

    verifyStorage(setup.packages, "beforeInstall");

    info("Installing packages");

    installPackages(setup.packages);

    info("Verifying storage");

    verifyStorage(setup.packages, "afterInstall");

    info("Checking origin directories");

    for (const origin of setup.origins) {
      let originDir = getRelativeFile(origin.oldPath);
      let exists = originDir.exists();
      ok(exists, "Origin directory does exist");

      let idbDir = originDir.clone();
      idbDir.append("idb");

      exists = idbDir.exists();
      ok(!exists, "idb directory doesn't exist");

      let metadataFile = originDir.clone();
      metadataFile.append(metadataFileName);

      exists = metadataFile.exists();
      ok(!exists, "Metadata file doesn't exist");

      if (origin.newPath) {
        originDir = getRelativeFile(origin.newPath);
        exists = originDir.exists();
        ok(!exists, "Origin directory doesn't exist");
      }
    }

    info("Initializing");

    let request = init(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Initialization succeeded");

    info("Verifying storage");

    verifyStorage(setup.packages, "afterInit");

    // TODO: Remove this block once temporary storage initialization is able to
    //       ignore unknown directories.
    getRelativeFile("storage/default/invalid+++example.com").remove(false);
    try {
      getRelativeFile("storage/temporary/invalid+++example.com").remove(false);
    } catch (ex) {}

    info("Checking origin directories");

    for (const origin of setup.origins) {
      let originDir = getRelativeFile(origin.oldPath);
      let exists = originDir.exists();
      ok(!exists, "Origin directory doesn't exist");

      if (origin.newPath) {
        originDir = getRelativeFile(origin.newPath);
        exists = originDir.exists();
        ok(exists, "Origin directory does exist");

        let idbDir = originDir.clone();
        idbDir.append("idb");

        exists = idbDir.exists();
        ok(exists, "idb directory does exist");

        let metadataFile = originDir.clone();
        metadataFile.append(metadataFileName);

        exists = metadataFile.exists();
        ok(exists, "Metadata file does exist");

        info("Initializing origin");

        let principal = getPrincipal(origin.url);
        request = initStorageAndOrigin(
          principal,
          origin.persistence,
          continueToNextStepSync
        );
        yield undefined;

        ok(request.resultCode == NS_OK, "Initialization succeeded");

        ok(!request.result, "Origin directory wasn't created");
      }
    }
  }

  finishTest();
}
