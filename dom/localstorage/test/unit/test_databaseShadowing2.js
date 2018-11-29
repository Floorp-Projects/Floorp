/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const url = "http://example.com";

  // The shadow database is prepared in test_databaseShadowing1.js

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", false);

  info("Verifying shadow database");

  let currentDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  let shadowDatabase = currentDir.clone();
  shadowDatabase.append("webappsstore.sqlite");

  let exists = shadowDatabase.exists();
  if (!exists) {
    finishTest();
    return;
  }

  info("Copying shadow database");

  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  shadowDatabase.copyTo(profileDir, "");

  info("Getting storage");

  let storage = getLocalStorage(getPrincipal(url));

  info("Verifying data");

  ok(storage.getItem("key0") == null, "Correct value");
  ok(storage.getItem("key1") == null, "Correct value");
  ok(storage.getItem("key2") == "value2", "Correct value");

  finishTest();
}
