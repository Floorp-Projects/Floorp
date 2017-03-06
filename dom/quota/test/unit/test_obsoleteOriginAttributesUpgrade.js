/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const origins = [
    {
      oldPath: "storage/permanent/moz-extension+++8ea6d31b-917c-431f-a204-15b95e904d4f^addonId=indexedDB-test%40kmaglione.mozilla.com",
      newPath: "storage/permanent/moz-extension+++8ea6d31b-917c-431f-a204-15b95e904d4f",
      url: "moz-extension://8ea6d31b-917c-431f-a204-15b95e904d4f",
      persistence: "persistent"
    },

    {
      oldPath: "storage/temporary/moz-extension+++8ea6d31b-917c-431f-a204-15b95e904d4f^addonId=indexedDB-test%40kmaglione.mozilla.com",
      newPath: "storage/temporary/moz-extension+++8ea6d31b-917c-431f-a204-15b95e904d4f",
      url: "moz-extension://8ea6d31b-917c-431f-a204-15b95e904d4f",
      persistence: "temporary"
    },

    {
      oldPath: "storage/default/moz-extension+++8ea6d31b-917c-431f-a204-15b95e904d4f^addonId=indexedDB-test%40kmaglione.mozilla.com",
      newPath: "storage/default/moz-extension+++8ea6d31b-917c-431f-a204-15b95e904d4f",
      url: "moz-extension://8ea6d31b-917c-431f-a204-15b95e904d4f",
      persistence: "default"
    }
  ];

  const metadataFileName = ".metadata-v2";

  let metadataBuffers = [];

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Installing package");

  installPackage("obsoleteOriginAttributes_profile");

  info("Checking origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.oldPath);
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

    metadataBuffers.push(fileReader.result);

    originDir = getRelativeFile(origin.newPath);
    exists = originDir.exists();
    ok(!exists, "Origin directory doesn't exist");
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

    originDir = getRelativeFile(origin.newPath);
    exists = originDir.exists();
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

    ok(!compareBuffers(metadataBuffer, metadataBuffers.shift()),
       "Metadata differ");

    info("Initializing origin");

    let principal = getPrincipal(origin.url);
    request =
      initOrigin(principal, origin.persistence, continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Initialization succeeded");

    ok(!request.result, "Origin directory wasn't created");
  }

  finishTest();
}
