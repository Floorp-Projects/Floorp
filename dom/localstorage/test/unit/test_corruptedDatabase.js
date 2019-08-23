/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const principal = getPrincipal("http://example.org");

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains one localStorage, all localStorage related files, a
  // script for localStorage creation and the storage database:
  // - storage/default/http+++example.org/ls
  // - storage/ls-archive.sqlite
  // - create_db.js
  // - storage.sqlite
  // - webappsstore.sqlite
  // The file create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/localstorage/test/unit/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Manually change first 6 chars in data.sqlite to "foobar".
  // 2. Remove the folder "storage/temporary".
  installPackage("corruptedDatabase_profile");

  let storage = getLocalStorage(principal);

  let length = storage.length;

  ok(length === 0, "Correct length");

  info("Resetting origin");

  request = resetOrigin(principal);
  await requestFinished(request);

  info("Getting usage");

  request = getOriginUsage(principal);
  await requestFinished(request);

  is(request.result.usage, 0, "Correct usage");
}
