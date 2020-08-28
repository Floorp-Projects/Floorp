/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that temporary storage initialization should
 * succeed while there is an origin directory that has an inconsistency between
 * its directory name and the origin name in its directory metadata file.
 */

async function testSteps() {
  const packages = ["originMismatch_profile", "defaultStorageDirectory_shared"];

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "beforeInstall");

  info("Installing package");

  // The profile contains:
  // - storage.sqlite (v2_3)
  // (A) Verify we are okay while the directory that we want to restore has
  // already existed.
  // - storage/default/http+++www.example.com/.metadata-v2
  //   (origin: http://www.example.com.)
  // - storage/default/http+++www.example.com/cache/.padding
  // - storage/default/http+++www.example.com./
  // (B) Verify restoring origin directory succeed.
  // - storage/default/http+++www.example.org/.metadata-v2
  //   (origin: http://www.example.org.)
  // - storage/default/http+++www.example.org/cache/.padding
  //
  // ToDo: Test case like:
  // - storage/default/http+++www.example.org(1)/.metadata-v2
  //   (origin: http://www.example.org)
  // - storage/default/http+++www.example.org/
  //
  // - storage/default/http+++www.foo.com/.metadata-v2
  //   (origin: http://www.bar.com)
  installPackages(packages);

  info("Verifying storage");

  verifyStorage(packages, "afterInstall");

  info("Initializing storage");

  request = init();
  await requestFinished(request);

  // ToDo: Remove this code once we support unknown directories in respository
  // (bug 1594075).
  let invalidDir = getRelativeFile("storage/default/invalid+++example.com");
  invalidDir.remove(true);
  invalidDir = getRelativeFile("storage/temporary/invalid+++example.com");
  invalidDir.remove(true);

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Verifying storage");

  verifyStorage(packages, "afterInitTemporaryStorage");

  request = clear();
  await requestFinished(request);
}
