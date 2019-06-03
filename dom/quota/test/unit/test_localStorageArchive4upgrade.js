/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that local storage directories are removed
 * during local storage archive upgrade from version 3 to version 4.
 * See bug 1549654.
 */

async function testSteps() {
  const lsDirs = [
    "storage/default/http+++localhost/ls",
    "storage/default/http+++www.mozilla.org/ls",
    "storage/default/http+++example.com/ls",
  ];

  const principalInfos = [
    "http://localhost",
    "http://www.mozilla.org",
    "http://example.com",
  ];

  const data = [
    { key: "foo0", value: "bar" },
    { key: "foo1", value: "A" },
    { key: "foo2", value: "A".repeat(100) },
  ];

  function getLocalStorage(principal) {
    return Services.domStorageManager.createStorage(null, principal, principal, "");
  }

  info("Setting pref");

  // xpcshell globals don't have associated clients in the Clients API sense, so
  // we need to disable client validation so that this unit test is allowed to
  // use LocalStorage.
  Services.prefs.setBoolPref("dom.storage.client_validation", false);

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
  // The file create_db.js in the package was run locally (with a build with
  // local storage archive version 3), specifically it was temporarily added to
  // xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/localstorage/test/unit/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Remove the folder "storage/temporary".
  installPackage("localStorageArchive4upgrade_profile");

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

  info("Getting storages");

  let storages = [];
  for (let i = 0; i < principalInfos.length; i++) {
    let storage = getLocalStorage(getPrincipal(principalInfos[i]));
    storages.push(storage);
  }

  info("Verifying data");

  for (let i = 0; i < storages.length; i++) {
    is(storages[i].getItem(data[i].key), data[i].value, "Correct value");
  }
}
