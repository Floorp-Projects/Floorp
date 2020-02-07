/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify
 * MaybeUpgradeFromIndexedDBDirectoryToPersistentStorageDirectory method.
 */

var testGenerator = testSteps();

function* testSteps() {
  const origins = [
    {
      oldPath: "indexedDB/http+++www.mozilla.org",
      newPath: "storage/default/http+++www.mozilla.org",
      url: "http://www.mozilla.org",
      persistence: "default",
    },

    {
      oldPath: "indexedDB/1007+f+app+++system.gaiamobile.org",
    },

    {
      oldPath: "indexedDB/1007+t+https+++developer.cdn.mozilla.net",
    },
  ];

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Installing package");

  // Storage used prior FF 26 (indexedDB/ directory).
  installPackage("indexedDBDirectory_profile");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.oldPath);
    let exists = originDir.exists();
    ok(exists, "Origin directory does exist");

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

  info("Checking origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.oldPath);
    let exists = originDir.exists();
    ok(!exists, "Origin directory doesn't exist");

    if (origin.newPath) {
      originDir = getRelativeFile(origin.newPath);
      exists = originDir.exists();
      ok(exists, "Origin directory does exist");

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
