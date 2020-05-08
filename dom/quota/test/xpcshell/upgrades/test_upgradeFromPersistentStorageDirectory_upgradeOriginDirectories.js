/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function* testSteps() {
  const origins = [
    {
      oldPath: "storage/persistent/1007+f+app+++system.gaiamobile.org",
      upgraded: true,
    },

    {
      oldPath: "storage/persistent/1007+t+https+++developer.cdn.mozilla.net",
      upgraded: true,
    },

    {
      oldPath: "storage/persistent/http+++www.mozilla.org",
      newPath: "storage/default/http+++www.mozilla.org",
      url: "http://www.mozilla.org",
      persistence: "default",
      upgraded: true,
    },
    {
      oldPath: "storage/persistent/http+++www.mozilla.org+8080",
      newPath: "storage/default/http+++www.mozilla.org+8080",
      url: "http://www.mozilla.org:8080",
      persistence: "default",
    },
  ];

  const metadataFileName = ".metadata";

  const packages = [
    // Storage used by FF 26-35 (storage/persistent/ directory with already
    // upgraded origin directories and not yet upgraded flat origin
    // directories).
    "persistentStorageDirectory_originDirectories_profile",
    "../persistentStorageDirectory_shared",
  ];

  clear(continueToNextStepSync);
  yield undefined;

  info("Verifying storage");

  verifyStorage(packages, "beforeInstall");

  info("Installing packages");

  installPackages(packages);

  info("Verifying storage");

  verifyStorage(packages, "afterInstall");

  info("Checking origin directories");

  for (const origin of origins) {
    let originDir = getRelativeFile(origin.oldPath);
    let exists = originDir.exists();
    ok(exists, "Origin directory does exist");

    let idbDir = originDir.clone();
    idbDir.append("idb");

    exists = idbDir.exists();
    if (origin.upgraded) {
      ok(exists, "idb directory does exist");
    } else {
      ok(!exists, "idb directory doesn't exist");
    }

    let metadataFile = originDir.clone();
    metadataFile.append(metadataFileName);

    exists = metadataFile.exists();
    if (origin.upgraded) {
      ok(exists, "Metadata file does exist");
    } else {
      ok(!exists, "Metadata file doesn't exist");
    }

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

  verifyStorage(packages, "afterInit");

  // TODO: Remove this block once temporary storage initialization and getting
  //       usage is able to ignore unknown directories.
  getRelativeFile("storage/default/invalid+++example.com").remove(false);
  getRelativeFile("storage/temporary/invalid+++example.com").remove(false);

  info("Checking origin directories");

  for (const origin of origins) {
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

  finishTest();
}
