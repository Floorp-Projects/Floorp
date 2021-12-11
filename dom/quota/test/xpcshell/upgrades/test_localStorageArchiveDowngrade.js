/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that local storage directories are removed
 * during local storage archive downgrade from any future version to current
 * version. See bug 1546305.
 */

async function testSteps() {
  const lsDirs = [
    "storage/default/http+++example.com/ls",
    "storage/default/http+++localhost/ls",
    "storage/default/http+++www.mozilla.org/ls",
  ];

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains three initialized origin directories with local
  // storage data, local storage archive, a script for origin initialization,
  // the storage database and the web apps store database:
  // - storage/default/https+++example.com
  // - storage/default/https+++localhost
  // - storage/default/https+++www.mozilla.org
  // - storage/ls-archive.sqlite
  // - create_db.js
  // - storage.sqlite
  // - webappsstore.sqlite
  // The file create_db.js in the package was run locally (with a build that
  // supports local storage archive upgrades and local storage archive version
  // set to max integer), specifically it was temporarily added to xpcshell.ini
  // and then executed:
  //   mach xpcshell-test --interactive dom/localstorage/test/xpcshell/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Remove the folder "storage/temporary".
  installPackage("localStorageArchiveDowngrade_profile");

  info("Checking ls dirs");

  for (let lsDir of lsDirs) {
    let dir = getRelativeFile(lsDir);

    exists = dir.exists();
    ok(exists, "ls directory does exist");
  }

  request = init();
  request = await requestFinished(request);

  info("Checking ls dirs");

  for (let lsDir of lsDirs) {
    let dir = getRelativeFile(lsDir);

    exists = dir.exists();
    ok(!exists, "ls directory doesn't exist");
  }
}
