/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that the old directory is removed in
 * MaybeUpgradeFromPersistentStorageDirectoryToDefaultStorageDirectory method.
 */

async function testSteps() {
  const url = "http://www.mozilla.org";
  const persistence = "default";
  const lastAccessed = 0x0005330925e07841;

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // Storage used by FF 36-48 (storage/default/ directory and re-created
  // storage/persistent/ directory by an older FF).
  installPackage("persistentAndDefaultStorageDirectory_profile");

  info("Checking directories");

  let persistentStorageDir = getRelativeFile("storage/persistent");
  let exists = persistentStorageDir.exists();
  ok(exists, "Persistent storage directory does exist");

  let defaultStorageDir = getRelativeFile("storage/default");
  exists = defaultStorageDir.exists();
  ok(exists, "Default storage directory does exist");

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Checking directories");

  persistentStorageDir = getRelativeFile("storage/persistent");
  exists = persistentStorageDir.exists();
  ok(!exists, "Persistent storage directory doesn't exist");

  defaultStorageDir = getRelativeFile("storage/default");
  exists = defaultStorageDir.exists();
  ok(exists, "Default storage directory does exist");

  info("Initializing origin");

  request = initStorageAndOrigin(getPrincipal(url), persistence);
  await requestFinished(request);

  ok(!request.result, "Origin directory wasn't created");

  info("Getting usage");

  request = getUsage(function() {}, /* getAll */ true);
  await requestFinished(request);

  info("Verifying result");

  const result = request.result;
  is(result.length, 1, "Correct number of usage results");

  info("Verifying usage result");

  const usageResult = result[0];
  ok(usageResult.origin == url, "Origin equals");
  ok(usageResult.lastAccessed == lastAccessed, "LastAccessed equals");
}
