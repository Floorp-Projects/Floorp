/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
  const data = {
    key: "foo",
    value: "",
  };

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains storage.sqlite and webappsstore.sqlite.
  // The archive migration_emptyValue_profile.zip was created by running
  // make_migration_emptyValue.js locally, specifically the special test was
  // temporarily activated in xpcshell.ini and then it was run as:
  // mach test --interactive dom/localstorage/test/unit/make_migration_emptyValue.js
  // Before packaging, additional manual steps are needed:
  // 1. Folder "cache2" is removed.
  // 2. Folder "crashes" is removed.
  // 3. File "mozinfo.json" is removed.
  installPackage("migration_emptyValue_profile");

  info("Getting storage");

  const storage = getLocalStorage();

  info("Verifying data");

  is(storage.getItem(data.key), data.value, "Correct value");
});
