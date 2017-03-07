/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const origins = [
    {
      path: "storage/default/1007+f+app+++system.gaiamobile.org",
      obsolete: true
    },

    {
      path: "storage/default/1007+t+https+++developer.cdn.mozilla.net",
      obsolete: true
    },

    {
      path: "storage/default/http+++www.mozilla.org",
      obsolete: false,
      url: "http://www.mozilla.org",
      persistence: "default"
    }
  ];

  const storageFileName = "storage.sqlite";
  const metadataFileName = ".metadata";
  const metadata2FileName = ".metadata-v2";

  // Test upgrade from FF 48 (no storage.sqlite and no .metadata-v2 files).

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  installPackage("originAttributesUpgrade_profile");

  info("Checking storage file");

  let storageFile = getRelativeFile(storageFileName);

  let exists = storageFile.exists();
  ok(!exists, "Storage file doesn't exist");

  info("Checking origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.path);

    exists = originDir.exists();
    ok(exists, "Origin directory does exist");

    let metadataFile = originDir.clone();
    metadataFile.append(metadataFileName);

    exists = metadataFile.exists();
    ok(exists, "Metadata file does exist");

    let metadata2File = originDir.clone();
    metadata2File.append(metadata2FileName);

    exists = metadata2File.exists();
    ok(!exists, "Metadata file doesn't exist");
  }

  info("Initializing");

  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  exists = storageFile.exists();
  ok(exists, "Storage file does exist");

  info("Checking origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.path);

    exists = originDir.exists();
    if (origin.obsolete) {
      ok(!exists, "Origin directory doesn't exist");
    } else {
      ok(exists, "Origin directory does exist");

      let metadataFile = originDir.clone();
      metadataFile.append(metadataFileName);

      exists = metadataFile.exists();
      ok(exists, "Metadata file does exist");

      let metadata2File = originDir.clone();
      metadata2File.append(metadata2FileName);

      exists = metadata2File.exists();
      ok(exists, "Metadata file does exist");

      info("Initializing origin");

      let principal = getPrincipal(origin.url);
      request =
        initOrigin(principal, origin.persistence, continueToNextStepSync);
      yield undefined;

      ok(request.resultCode == NS_OK, "Initialization succeeded");

      ok(!request.result, "Origin directory wasn't created");
    }
  }

  finishTest();
}
