/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function* testSteps() {
  const tempMetadataFiles = [
    "storage/permanent/chrome/.metadata-tmp",
    "storage/permanent/chrome/.metadata-v2-tmp",
  ];

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Installing package");

  installPackage("tempMetadataCleanup_profile");

  info("Initializing origin");

  let request = initStorageAndChromeOrigin(
    "persistent",
    continueToNextStepSync
  );
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  ok(!request.result, "Origin directory wasn't created");

  for (let tempMetadataFile of tempMetadataFiles) {
    info("Checking temp metadata file");

    let file = getRelativeFile(tempMetadataFile);

    let exists = file.exists();
    ok(!exists, "Temp metadata file doesn't exist");
  }

  finishTest();
}
