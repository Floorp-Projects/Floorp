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

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // Storage used by FF 26-35 (storage/persistent/ directory and re-created
  // indexedDB directory by an older FF).
  installPackage("indexedDBAndPersistentStorageDirectory_profile");

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
