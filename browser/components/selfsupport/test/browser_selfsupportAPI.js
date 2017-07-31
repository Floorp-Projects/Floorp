function prefHas(pref) {
  return Services.prefs.getPrefType(pref) != Ci.nsIPrefBranch.PREF_INVALID;
}

function prefIsSet(pref) {
  return Services.prefs.prefHasUserValue(pref);
}

function test_resetPref() {
  const prefNewName = "browser.newpref.fake";
  Assert.ok(!prefHas(prefNewName), "pref should not exist");

  const prefExistingName = "extensions.hotfix.id";
  Assert.ok(prefHas(prefExistingName), "pref should exist");
  Assert.ok(!prefIsSet(prefExistingName), "pref should not be user-set");
  let prefExistingOriginalValue = Services.prefs.getStringPref(prefExistingName);

  registerCleanupFunction(function() {
    Services.prefs.setStringPref(prefExistingName, prefExistingOriginalValue);
    Services.prefs.deleteBranch(prefNewName);
  });

  // 1. do nothing on an inexistent pref
  MozSelfSupport.resetPref(prefNewName);
  Assert.ok(!prefHas(prefNewName), "pref should still not exist");

  // 2. creation of a new pref
  Services.prefs.setIntPref(prefNewName, 10);
  Assert.ok(prefHas(prefNewName), "pref should exist");
  Assert.equal(Services.prefs.getIntPref(prefNewName), 10, "pref value should be 10");

  MozSelfSupport.resetPref(prefNewName);
  Assert.ok(!prefHas(prefNewName), "pref should not exist any more");

  // 3. do nothing on an unchanged existing pref
  MozSelfSupport.resetPref(prefExistingName);
  Assert.ok(prefHas(prefExistingName), "pref should still exist");
  Assert.equal(Services.prefs.getStringPref(prefExistingName), prefExistingOriginalValue, "pref value should be the same as original");

  // 4. change the value of an existing pref
  Services.prefs.setStringPref(prefExistingName, "anyone@mozilla.org");
  Assert.ok(prefHas(prefExistingName), "pref should exist");
  Assert.equal(Services.prefs.getStringPref(prefExistingName), "anyone@mozilla.org", "pref value should have changed");

  MozSelfSupport.resetPref(prefExistingName);
  Assert.ok(prefHas(prefExistingName), "pref should still exist");
  Assert.equal(Services.prefs.getStringPref(prefExistingName), prefExistingOriginalValue, "pref value should be the same as original");

  // 5. delete an existing pref
  // deleteBranch is implemented in such a way that
  // clearUserPref can't undo its action
  // see discussion in bug 1075160
}

function test_resetSearchEngines() {
  const defaultEngineOriginal = Services.search.defaultEngine;
  const visibleEnginesOriginal = Services.search.getVisibleEngines();

  // 1. do nothing on unchanged search configuration
  MozSelfSupport.resetSearchEngines();
  Assert.equal(Services.search.defaultEngine, defaultEngineOriginal, "default engine should be reset");
  Assert.deepEqual(Services.search.getVisibleEngines(), visibleEnginesOriginal,
                   "default visible engines set should be reset");

  // 2. change the default search engine
  const defaultEngineNew = visibleEnginesOriginal[3];
  Assert.notEqual(defaultEngineOriginal, defaultEngineNew, "new default engine should be different from original");
  Services.search.defaultEngine = defaultEngineNew;
  Assert.equal(Services.search.defaultEngine, defaultEngineNew, "default engine should be set to new");
  MozSelfSupport.resetSearchEngines();
  Assert.equal(Services.search.defaultEngine, defaultEngineOriginal, "default engine should be reset");
  Assert.deepEqual(Services.search.getVisibleEngines(), visibleEnginesOriginal,
                   "default visible engines set should be reset");

  // 3. remove an engine
  const engineRemoved = visibleEnginesOriginal[2];
  Services.search.removeEngine(engineRemoved);
  Assert.ok(Services.search.getVisibleEngines().indexOf(engineRemoved) == -1,
            "removed engine should not be visible any more");
  MozSelfSupport.resetSearchEngines();
  Assert.equal(Services.search.defaultEngine, defaultEngineOriginal, "default engine should be reset");
  Assert.deepEqual(Services.search.getVisibleEngines(), visibleEnginesOriginal,
                   "default visible engines set should be reset");

  // 4. add an angine
  // we don't remove user-added engines as they are only used if selected
}

function test() {
  test_resetPref();
  test_resetSearchEngines();
}
