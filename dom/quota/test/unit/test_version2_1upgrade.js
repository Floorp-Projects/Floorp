/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const origins = [
    "storage/default/chrome/",
    "storage/default/http+++www.mozilla.org/"
  ];
  const paddingFilePath = "cache/.padding";

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  // The profile contains two cache storages:
  // - storage/default/chrome/cache,
  // - storage/default/http+++www.mozilla.org/cache
  // The file create_cache.js in the package was run locally, specifically it
  // was temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/quota/test/unit/create_cache.js
  // Note: it only creates the directory "storage/default/chrome/cache".
  // To make it become the profile in the test, two more manual steps are
  // needed.
  // 1. Remove the folder "storage/temporary".
  // 2. Copy the content under the "storage/default/chrome" to
  //    "storage/default/http+++www.mozilla.org".
  // 3. Manually create an asmjs folder under the
  //    "storage/default/http+++www.mozilla.org/".
  installPackage("version2_1upgrade_profile");

  info("Checking padding file before upgrade (QM version 2.0)");

  for (let origin of origins) {
    let paddingFile = getRelativeFile(origin + paddingFilePath);

    let exists = paddingFile.exists();
    ok(!exists, "Padding file doesn't exist");
  }

  info("Initializing");

  // Initialize QuotaManager to trigger upgrade the QM to version 2.1
  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Checking padding files after upgrade (QM version 2.1)");

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
