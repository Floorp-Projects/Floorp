/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify UpgradeCacheFrom1To2 method.
 */

async function testSteps() {
  const packages = [
    // Storage used prior FF 88 (cache version 1).
    // The profile contains one initialized origin directory with simple
    // database data, a script for origin initialization and the storage
    // database:
    // - storage/default/https+++www.mozilla.org^userContextId=1
    // - create_db.js
    // - storage.sqlite
    // The file create_db.js in the package was run locally, specifically it was
    // temporarily added to xpcshell.ini and then executed:
    //   mach xpcshell-test dom/quota/test/xpcshell/upgrades/create_db.js
    //   --interactive
    // Note: to make it become the profile in the test, additional manual steps
    // are needed.
    // 1. Remove the folder "storage/temporary".
    // 2. Remove the file "storage/ls-archive.sqlite".
    "cacheVersion1_profile",
    "../defaultStorageDirectory_shared",
  ];
  const principal = getPrincipal("https://www.mozilla.org", {
    userContextId: 1,
  });
  const originUsage = 100;

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "beforeInstall");

  info("Installing package");

  installPackages(packages);

  info("Verifying storage");

  verifyStorage(packages, "afterInstall");

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "afterInit");

  // TODO: Remove this block once temporary storage initialization is able to
  //       ignore unknown directories.
  getRelativeFile("storage/default/invalid+++example.com").remove(false);
  getRelativeFile("storage/temporary/invalid+++example.com").remove(false);

  info("Initializing temporary storage");

  request = initTemporaryStorage(continueToNextStepSync);
  await requestFinished(request);

  info("Getting origin usage");

  request = getOriginUsage(principal, /* fromMemory */ true);
  await requestFinished(request);

  info("Verifying origin usage");

  is(request.result.usage, originUsage, "Correct origin usage");
}
