/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function* testSteps() {
  const storageFile = "storage.sqlite";

  const metadataFiles = [
    {
      path: "storage/permanent/chrome/.metadata",
      shouldExistAfterInit: false,
    },

    {
      path: "storage/permanent/chrome/.metadata-tmp",
      shouldExistAfterInit: false,
    },

    {
      path: "storage/permanent/chrome/.metadata-v2",
      shouldExistAfterInit: true,
    },

    {
      path: "storage/permanent/chrome/.metadata-v2-tmp",
      shouldExistAfterInit: false,
    },
  ];

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Verifying initialization status");

  verifyInitializationStatus(false, false).then(continueToNextStepSync);
  yield undefined;

  info("Getting usage");

  getCurrentUsage(grabUsageAndContinueHandler);
  let usage = yield undefined;

  ok(usage == 0, "Usage is zero");

  info("Verifying initialization status");

  verifyInitializationStatus(true, false).then(continueToNextStepSync);
  yield undefined;

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Verifying initialization status");

  verifyInitializationStatus(false, false).then(continueToNextStepSync);
  yield undefined;

  info("Installing package");

  // The profile contains just one empty IndexedDB database. The file
  // create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  // mach xpcshell-test --interactive dom/quota/test/xpcshell/create_db.js
  installPackage("basics_profile");

  info("Getting usage");

  getCurrentUsage(grabUsageAndContinueHandler);
  usage = yield undefined;

  ok(usage > 0, "Usage is not zero");

  info("Verifying initialization status");

  verifyInitializationStatus(true, false).then(continueToNextStepSync);
  yield undefined;

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Checking storage file");

  let file = getRelativeFile(storageFile);

  let exists = file.exists();
  ok(!exists, "Storage file doesn't exist");

  info("Verifying initialization status");

  verifyInitializationStatus(false, false).then(continueToNextStepSync);
  yield undefined;

  info("Initializing");

  request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  exists = file.exists();
  ok(exists, "Storage file does exist");

  info("Verifying initialization status");

  verifyInitializationStatus(true, false).then(continueToNextStepSync);
  yield undefined;

  info("Initializing origin");

  request = initStorageAndChromeOrigin("persistent", continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  ok(request.result, "Origin directory was created");

  for (let metadataFile of metadataFiles) {
    file = getRelativeFile(metadataFile.path);

    exists = file.exists();

    if (metadataFile.shouldExistAfterInit) {
      ok(exists, "Metadata file does exist");
    } else {
      ok(!exists, "Metadata file doesn't exist");
    }
  }

  info("Verifying initialization status");

  verifyInitializationStatus(true, false).then(continueToNextStepSync);

  yield undefined;

  finishTest();
}
