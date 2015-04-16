Cu.import("resource://gre/modules/Preferences.jsm");

function test() {
  const prefNewName = "browser.newpref.fake";
  Assert.ok(!Preferences.has(prefNewName), "pref should not exist");

  const prefExistingName = "extensions.hotfix.id";
  Assert.ok(Preferences.has(prefExistingName), "pref should exist");
  Assert.ok(!Preferences.isSet(prefExistingName), "pref should not be user-set");
  let prefExistingOriginalValue = Preferences.get(prefExistingName);

  registerCleanupFunction(function() {
    Preferences.set(prefExistingName, prefExistingOriginalValue);
    Services.prefs.deleteBranch(prefNewName);
  });

  // 1. do nothing on an inexistent pref
  MozSelfSupport.resetPref(prefNewName);
  Assert.ok(!Preferences.has(prefNewName), "pref should still not exist");

  // 2. creation of a new pref
  Preferences.set(prefNewName, 10);
  Assert.ok(Preferences.has(prefNewName), "pref should exist");
  Assert.equal(Preferences.get(prefNewName), 10, "pref value should be 10");

  MozSelfSupport.resetPref(prefNewName);
  Assert.ok(!Preferences.has(prefNewName), "pref should not exist any more");

  // 3. do nothing on an unchanged existing pref
  MozSelfSupport.resetPref(prefExistingName);
  Assert.ok(Preferences.has(prefExistingName), "pref should still exist");
  Assert.equal(Preferences.get(prefExistingName), prefExistingOriginalValue, "pref value should be the same as original");

  // 4. change the value of an existing pref
  Preferences.set(prefExistingName, "anyone@mozilla.org");
  Assert.ok(Preferences.has(prefExistingName), "pref should exist");
  Assert.equal(Preferences.get(prefExistingName), "anyone@mozilla.org", "pref value should have changed");

  MozSelfSupport.resetPref(prefExistingName);
  Assert.ok(Preferences.has(prefExistingName), "pref should still exist");
  Assert.equal(Preferences.get(prefExistingName), prefExistingOriginalValue, "pref value should be the same as original");

  // 5. delete an existing pref
  // deleteBranch is implemented in such a way that
  // clearUserPref can't undo its action
  // see discussion in bug 1075160
}
