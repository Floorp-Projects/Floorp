/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const url = "http://example.com";

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains one initialized origin directory with local
  // storage data, local storage archive, a script for origin initialization,
  // the storage database and the web apps store database:
  // - storage/default/http+++example.com
  // - storage/ls-archive.sqlite
  // - create_db.js
  // - storage.sqlite
  // - webappsstore.sqlite
  // The file create_db.js in the package was run locally (with a build with
  // local storage archive version 1 and database schema version 2),
  // specifically it was temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/localstorage/test/unit/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Remove the folder "storage/temporary".
  installPackage("schema3upgrade_profile");

  let storage = getLocalStorage(getPrincipal(url));
  storage.open();
}
