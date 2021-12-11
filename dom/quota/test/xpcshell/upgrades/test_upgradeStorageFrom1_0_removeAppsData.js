/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify MaybeRemoveAppsData method.
 */

function* testSteps() {
  const origins = [
    {
      path: "storage/default/http+++www.mozilla.org",
      obsolete: false,
    },

    {
      path: "storage/default/app+++system.gaiamobile.org^appId=1007",
      obsolete: true,
    },

    {
      path:
        "storage/default/https+++developer.cdn.mozilla.net^appId=1007&inBrowser=1",
      obsolete: true,
    },
  ];

  const packages = [
    // Storage used by FF 49-54 (storage version 1.0 with apps data).
    "version1_0_appsData_profile",
    "../defaultStorageDirectory_shared",
  ];

  info("Clearing");

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
    let originDir = getRelativeFile(origin.path);

    let exists = originDir.exists();
    ok(exists, "Origin directory does exist");
  }

  info("Initializing");

  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Verifying storage");

  verifyStorage(packages, "afterInit");

  // TODO: Remove this block once getting usage is able to ignore unknown
  //       directories.
  getRelativeFile("storage/default/invalid+++example.com").remove(false);
  getRelativeFile("storage/permanent/invalid+++example.com").remove(false);
  getRelativeFile("storage/temporary/invalid+++example.com").remove(false);

  info("Checking origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.path);

    let exists = originDir.exists();
    if (origin.obsolete) {
      ok(!exists, "Origin directory doesn't exist");
    } else {
      ok(exists, "Origin directory does exist");
    }
  }

  info("Getting usage");

  getUsage(grabResultAndContinueHandler, /* getAll */ true);
  let result = yield undefined;

  info("Verifying result");

  is(result.length, 1, "Correct number of usage results");

  finishTest();
}
