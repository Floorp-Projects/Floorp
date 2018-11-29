/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const url = "http://example.com";

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  let principal = getPrincipal(url);

  info("Getting storage");

  let storage = getLocalStorage(principal);

  info("Adding data");

  storage.setItem("key0", "value0");
  storage.clear();
  storage.setItem("key1", "value1");
  storage.removeItem("key1");
  storage.setItem("key2", "value2");

  info("Closing storage");

  storage.close();

  resetOrigin(principal, continueToNextStepSync);
  yield undefined;

  info("Verifying shadow database");

  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let shadowDatabase = profileDir.clone();
  shadowDatabase.append("webappsstore.sqlite");

  let exists = shadowDatabase.exists();
  ok(exists, "Shadow database does exist");

  info("Copying shadow database");

  let currentDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  shadowDatabase.copyTo(currentDir, "");

  // The shadow database is now prepared for test_databaseShadowing2.js

  finishTest();
}
