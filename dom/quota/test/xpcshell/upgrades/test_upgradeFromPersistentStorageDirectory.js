/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify
 * MaybeUpgradeFromPersistentStorageDirectoryToDefaultStorageDirectory method.
 */

loadScript("dom/quota/test/common/file.js");

function* testSteps() {
  const origins = [
    {
      oldPath: "storage/persistent/1007+f+app+++system.gaiamobile.org",
    },

    {
      oldPath: "storage/persistent/1007+t+https+++developer.cdn.mozilla.net",
    },

    {
      oldPath: "storage/persistent/chrome",
      newPath: "storage/permanent/chrome",
      chrome: true,
      persistence: "persistent",
    },

    {
      oldPath: "storage/persistent/file++++",
      newPath: "storage/default/file++++",
      url: "file:///",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++++index.html",
      newPath: "storage/default/file++++++index.html",
      url: "file:///+/index.html",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++++index.html",
      newPath: "storage/default/file++++++index.html",
      url: "file://///index.html",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++Users+joe+",
      newPath: "storage/default/file++++Users+joe+",
      url: "file:///Users/joe/",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++Users+joe+c+++index.html",
      newPath: "storage/default/file++++Users+joe+c+++index.html",
      url: "file:///Users/joe/c++/index.html",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++Users+joe+c+++index.html",
      newPath: "storage/default/file++++Users+joe+c+++index.html",
      url: "file:///Users/joe/c///index.html",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++Users+joe+index.html",
      newPath: "storage/default/file++++Users+joe+index.html",
      url: "file:///Users/joe/index.html",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++c++",
      newPath: "storage/default/file++++c++",
      url: "file:///c:/",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++c++Users+joe+",
      newPath: "storage/default/file++++c++Users+joe+",
      url: "file:///c:/Users/joe/",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/file++++c++Users+joe+index.html",
      newPath: "storage/default/file++++c++Users+joe+index.html",
      url: "file:///c:/Users/joe/index.html",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/http+++127.0.0.1",
      newPath: "storage/default/http+++127.0.0.1",
      url: "http://127.0.0.1",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/http+++localhost",
      newPath: "storage/default/http+++localhost",
      url: "http://localhost",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/http+++www.mozilla.org",
      newPath: "storage/default/http+++www.mozilla.org",
      url: "http://www.mozilla.org",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/http+++www.mozilla.org+8080",
      newPath: "storage/default/http+++www.mozilla.org+8080",
      url: "http://www.mozilla.org:8080",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/https+++www.mozilla.org",
      newPath: "storage/default/https+++www.mozilla.org",
      url: "https://www.mozilla.org",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/https+++www.mozilla.org+8080",
      newPath: "storage/default/https+++www.mozilla.org+8080",
      url: "https://www.mozilla.org:8080",
      persistence: "default",
    },

    {
      oldPath: "storage/persistent/indexeddb+++fx-devtools",
      newPath: "storage/permanent/indexeddb+++fx-devtools",
      url: "indexeddb://fx-devtools",
      persistence: "persistent",
    },

    {
      oldPath: "storage/persistent/moz-safe-about+++home",
    },

    {
      oldPath: "storage/persistent/moz-safe-about+home",
      newPath: "storage/permanent/moz-safe-about+home",
      url: "moz-safe-about:home",
      persistence: "persistent",
    },

    {
      oldPath:
        "storage/persistent/resource+++fx-share-addon-at-mozilla-dot-org-fx-share-addon-data",
      newPath:
        "storage/permanent/resource+++fx-share-addon-at-mozilla-dot-org-fx-share-addon-data",
      url: "resource://fx-share-addon-at-mozilla-dot-org-fx-share-addon-data",
      persistence: "persistent",
    },

    {
      oldPath: "storage/temporary/1007+f+app+++system.gaiamobile.org",
    },

    {
      oldPath: "storage/temporary/1007+t+https+++developer.cdn.mozilla.net",
    },

    // The .metadata file was intentionally appended for this origin directory
    // to test recovery from unfinished upgrades (some metadata files can be
    // already upgraded).
    {
      oldPath: "storage/temporary/chrome",
      newPath: "storage/temporary/chrome",
      metadataUpgraded: true,
      chrome: true,
      persistence: "temporary",
    },

    {
      oldPath: "storage/temporary/http+++localhost",
      newPath: "storage/temporary/http+++localhost",
      url: "http://localhost",
      persistence: "temporary",
    },

    // The .metadata file was intentionally removed for this origin directory
    // to test restoring during upgrade.
    {
      oldPath: "storage/temporary/http+++localhost+81",
      newPath: "storage/temporary/http+++localhost+81",
      metadataRemoved: true,
      url: "http://localhost:81",
      persistence: "temporary",
    },

    // The .metadata file was intentionally truncated for this origin directory
    // to test restoring during upgrade.
    {
      oldPath: "storage/temporary/http+++localhost+82",
      newPath: "storage/temporary/http+++localhost+82",
      url: "http://localhost:82",
      persistence: "temporary",
    },
  ];

  const metadataFileName = ".metadata";

  const packages = [
    // Storage used by 26-35 (storage/persistent/ directory, tracked only
    // timestamp in .metadata for persistent storage and isApp not tracked in
    // .metadata for temporary storage).
    "persistentStorageDirectory_profile",
    "../persistentStorageDirectory_shared",
  ];

  let metadataBuffers = [];

  clear(continueToNextStepSync);
  yield undefined;

  info("Verifying storage");

  verifyStorage(packages, "beforeInstall");

  info("Installing packages");

  installPackages(packages);

  info("Verifying storage");

  verifyStorage(packages, "afterInstall");

  info("Checking origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.oldPath);
    let exists = originDir.exists();
    ok(exists, "Origin directory does exist");

    if (origin.newPath) {
      info("Reading out contents of metadata file");

      let metadataFile = originDir.clone();
      metadataFile.append(metadataFileName);

      if (origin.metadataRemoved) {
        metadataBuffers.push(new ArrayBuffer(0));
      } else {
        File.createFromNsIFile(metadataFile).then(grabArgAndContinueHandler);
        let file = yield undefined;

        let fileReader = new FileReader();
        fileReader.onload = continueToNextStepSync;
        fileReader.readAsArrayBuffer(file);

        yield undefined;

        metadataBuffers.push(fileReader.result);
      }

      if (origin.newPath != origin.oldPath) {
        originDir = getRelativeFile(origin.newPath);
        exists = originDir.exists();
        ok(!exists, "Origin directory doesn't exist");
      }
    }
  }

  info("Initializing");

  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Verifying storage");

  verifyStorage(packages, "afterInit");

  // TODO: Remove this block once temporary storage initialization is able to
  //       ignore unknown directories.
  getRelativeFile("storage/default/invalid+++example.com").remove(false);
  getRelativeFile("storage/temporary/invalid+++example.com").remove(false);

  for (let origin of origins) {
    if (!origin.newPath || origin.newPath != origin.oldPath) {
      let originDir = getRelativeFile(origin.oldPath);
      let exists = originDir.exists();
      ok(!exists, "Origin directory doesn't exist");
    }

    if (origin.newPath) {
      let originDir = getRelativeFile(origin.newPath);
      let exists = originDir.exists();
      ok(exists, "Origin directory does exist");

      info("Reading out contents of metadata file");

      let metadataFile = originDir.clone();
      metadataFile.append(metadataFileName);

      File.createFromNsIFile(metadataFile).then(grabArgAndContinueHandler);
      let file = yield undefined;

      let fileReader = new FileReader();
      fileReader.onload = continueToNextStepSync;
      fileReader.readAsArrayBuffer(file);

      yield undefined;

      let metadataBuffer = fileReader.result;

      info("Verifying blobs differ");

      if (origin.metadataUpgraded) {
        ok(
          compareBuffers(metadataBuffer, metadataBuffers.shift()),
          "Metadata doesn't differ"
        );
      } else {
        ok(
          !compareBuffers(metadataBuffer, metadataBuffers.shift()),
          "Metadata differ"
        );
      }

      info("Initializing origin");

      if (origin.chrome) {
        request = initStorageAndChromeOrigin(
          origin.persistence,
          continueToNextStepSync
        );
        yield undefined;
      } else {
        let principal = getPrincipal(origin.url);
        request = initStorageAndOrigin(
          principal,
          origin.persistence,
          continueToNextStepSync
        );
        yield undefined;
      }

      ok(request.resultCode == NS_OK, "Initialization succeeded");

      ok(!request.result, "Origin directory wasn't created");
    }
  }

  finishTest();
}
