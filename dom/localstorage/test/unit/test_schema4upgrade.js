/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
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
  // - storage.sqlite
  // - test_create_db.js
  // - webappsstore.sqlite
  //
  // The file test_create_db.js in the package was run locally by
  // adding it temporarily to xpcshell.ini and then executed with
  //   mach xpcshell-test --headless dom/localstorage/test/unit/test_create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Remove the folder "storage/temporary".
  installPackage("schema4upgrade_profile");

  let storage = getLocalStorage(getPrincipal(url));
  storage.open();
});
