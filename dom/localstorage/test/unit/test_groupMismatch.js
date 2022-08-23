/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that metadata files with old group information
 * get updated, so writing to local storage won't cause a crash because of null
 * quota object. See bug 1516333.
 */

add_task(async function testSteps() {
  const principal = getPrincipal("https://foo.bar.mozilla-iot.org");

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains one initialized origin directory, a script for origin
  // initialization and the storage database:
  // - storage/default/https+++foo.bar.mozilla-iot.org
  // - create_db.js
  // - storage.sqlite
  // The file create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/localstorage/test/unit/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Manually change the group in .metadata and .metadata-v2 from
  //    "bar.mozilla-iot.org" to "mozilla-iot.org".
  // 2. Remove the folder "storage/temporary".
  // 3. Remove the file "storage/ls-archive.sqlite".
  installPackage("groupMismatch_profile");

  info("Getting storage");

  let storage = getLocalStorage(principal);

  info("Adding item");

  storage.setItem("foo", "bar");
});
