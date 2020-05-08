/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify MaybeRemoveMorgueDirectory method.
 */

function* testSteps() {
  const morgueFile = "storage/default/http+++example.com/morgue";

  const packages = [
    // Storage used by FF 49-54 (storage version 1.0 with morgue directory).
    "version1_0_morgueDirectory_profile",
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

  info("Checking morgue file");

  let file = getRelativeFile(morgueFile);

  let exists = file.exists();
  ok(exists, "Morgue file does exist");

  info("Initializing");

  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Verifying storage");

  verifyStorage(packages, "afterInit");

  info("Checking morgue file");

  exists = file.exists();
  ok(!exists, "Morgue file doesn't exist");

  finishTest();
}
