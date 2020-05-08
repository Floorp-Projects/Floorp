/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify UpgradeStorageFrom2_0To2_1 method.
 */

function* testSteps() {
  const origins = [
    "storage/default/chrome/",
    "storage/default/http+++www.mozilla.org/",
  ];
  const paddingFilePath = "cache/.padding";

  const packages = [
    // Storage used by FF 55-56 (storage version 2.0).
    // The profile contains two cache storages:
    // - storage/default/chrome/cache,
    // - storage/default/http+++www.mozilla.org/cache
    // The file create_cache.js in the package was run locally, specifically it
    // was temporarily added to xpcshell.ini and then executed:
    //   mach xpcshell-test --interactive dom/quota/test/xpcshell/create_cache.js
    // Note: it only creates the directory "storage/default/chrome/cache".
    // To make it become the profile in the test, two more manual steps are
    // needed.
    // 1. Remove the folder "storage/temporary".
    // 2. Copy the content under the "storage/default/chrome" to
    //    "storage/default/http+++www.mozilla.org".
    // 3. Manually create an asmjs folder under the
    //    "storage/default/http+++www.mozilla.org/".
    "version2_0_profile",
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

  info("Checking padding files before upgrade (storage version 2.0)");

  for (let origin of origins) {
    let paddingFile = getRelativeFile(origin + paddingFilePath);
    let exists = paddingFile.exists();
    ok(!exists, "Padding file doesn't exist");
  }

  info("Initializing");

  // Initialize to trigger storage upgrade from version 2.0.
  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Verifying storage");

  verifyStorage(packages, "afterInit");

  info("Checking padding files after upgrade");

  for (let origin of origins) {
    let paddingFile = getRelativeFile(origin + paddingFilePath);
    let exists = paddingFile.exists();
    ok(exists, "Padding file does exist");

    info("Reading out contents of padding file");

    File.createFromNsIFile(paddingFile).then(grabArgAndContinueHandler);
    let domFile = yield undefined;

    let fileReader = new FileReader();
    fileReader.onload = continueToNextStepSync;
    fileReader.readAsArrayBuffer(domFile);
    yield undefined;

    let paddingFileInfo = new Float64Array(fileReader.result);
    ok(paddingFileInfo.length == 1, "Padding file does take 64 bytes.");
    ok(paddingFileInfo[0] == 0, "Padding size does default to zero.");
  }

  finishTest();
}
