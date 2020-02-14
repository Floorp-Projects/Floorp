/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that the old directory is removed in
 * MaybeUpgradeFromIndexedDBDirectoryToPersistentStorageDirectory method.
 */

async function testSteps() {
  const url = "http://www.mozilla.org";
  const persistence = "default";

  const packages = [
    // Storage used by FF 26-35 (storage/persistent/ directory and re-created
    // indexedDB directory by an older FF).
    "indexedDBAndPersistentStorageDirectory_profile",
    "../persistentStorageDirectory_shared",
  ];

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "beforeInstall");

  info("Installing packages");

  installPackages(packages);

  info("Verifying storage");

  verifyStorage(packages, "afterInstall");

  info("Checking directories");

  let indexedDBDir = getRelativeFile("indexedDB");
  let exists = indexedDBDir.exists();
  ok(exists, "IndexedDB directory does exist");

  let persistentStorageDir = getRelativeFile("storage/persistent");
  exists = persistentStorageDir.exists();
  ok(exists, "Persistent storage directory does exist");

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "afterInit");

  // TODO: Remove this block once temporary storage initialization is able to
  //       ignore unknown directories.
  getRelativeFile("storage/default/invalid+++example.com").remove(false);
  getRelativeFile("storage/temporary/invalid+++example.com").remove(false);

  info("Checking directories");

  indexedDBDir = getRelativeFile("indexedDB");
  exists = indexedDBDir.exists();
  ok(!exists, "IndexedDB directory doesn't exist");

  // FF 36 renamed storage/persistent/ to storage/default/ so it can't exist
  // either.
  persistentStorageDir = getRelativeFile("storage/persistent");
  exists = persistentStorageDir.exists();
  ok(!exists, "Persistent storage directory doesn't exist");

  info("Initializing origin");

  request = initStorageAndOrigin(getPrincipal(url), persistence);
  await requestFinished(request);

  ok(!request.result, "Origin directory wasn't created");
}
