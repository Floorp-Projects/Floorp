// The various properties that we'll be testing
var testdata = {
  missing: "fuel.fuel-test-missing",
  dummy: "fuel.fuel-test",
  string: "browser.active_color",
  integer: "permissions.default.image",
  boolean: "browser.underline_anchors"
};

function test() {
  // test getting nonexistent values
  var itemValue = Application.prefs.getValue(testdata.missing, "default");
  is(itemValue, "default", "Check 'Application.prefs.getValue' for nonexistent item");

  is(Application.prefs.get(testdata.missing), null, "Check 'Application.prefs.get' for nonexistent item");

  // test setting and getting a value
  Application.prefs.setValue(testdata.dummy, "dummy");
  itemValue = Application.prefs.getValue(testdata.dummy, "default");
  is(itemValue, "dummy", "Check 'Application.prefs.getValue' for existing item");

  // test for overwriting an existing value
  Application.prefs.setValue(testdata.dummy, "smarty");
  itemValue = Application.prefs.getValue(testdata.dummy, "default");
  is(itemValue, "smarty", "Check 'Application.prefs.getValue' for overwritten item");

  // test setting and getting a value
  Application.prefs.get(testdata.dummy).value = "dummy2";
  itemValue = Application.prefs.get(testdata.dummy).value;
  is(itemValue, "dummy2", "Check 'Application.prefs.get().value' for existing item");

  // test resetting a pref [since there is no default value, the pref should disappear]
  Application.prefs.get(testdata.dummy).reset();
  itemValue = Application.prefs.getValue(testdata.dummy, "default");
  is(itemValue, "default", "Check 'Application.prefs.getValue' for reset pref");

  // test to see if a non-existant property exists
  ok(!Application.prefs.has(testdata.dummy), "Check non-existant property for existence");

  // PREF: string browser.active_color == #EE0000

  // test to see if an existing string property exists
  ok(Application.prefs.has(testdata.string), "Check existing string property for existence");

  // test accessing a non-existant string property
  var val = Application.prefs.getValue(testdata.dummy, "default");
  is(val, "default", "Check non-existant string property for expected value");

  // test accessing an existing string property
  var val = Application.prefs.getValue(testdata.string, "default");
  is(val, "#EE0000", "Check existing string property for expected value");

  // test manipulating an existing string property
  Application.prefs.setValue(testdata.string, "#EF0000");
  var val = Application.prefs.getValue(testdata.string, "default");
  is(val, "#EF0000", "Set existing string property");

  // test getting the type of an existing string property
  var type = Application.prefs.get(testdata.string).type;
  is(type, "String", "Check 'Application.prefs.get().type' for string pref");

  // test resetting an existing string property
  Application.prefs.get(testdata.string).reset();
  var val = Application.prefs.getValue(testdata.string, "default");
  is(val, "#EE0000", "Reset existing string property");

  // PREF: integer permissions.default.image == 1

  // test to see if an existing integer property exists
  ok(Application.prefs.has(testdata.integer), "Check existing integer property for existence");

  // test accessing a non-existant integer property
  var val = Application.prefs.getValue(testdata.dummy, 0);
  is(val, 0, "Check non-existant integer property for expected value");

  // test accessing an existing integer property
  var val = Application.prefs.getValue(testdata.integer, 0);
  is(val, 1, "Check existing integer property for expected value");

  // test manipulating an existing integer property
  Application.prefs.setValue(testdata.integer, 0);
  var val = Application.prefs.getValue(testdata.integer, 1);
  is(val, 0, "Set existing integer property");

  // test getting the type of an existing integer property
  var type = Application.prefs.get(testdata.integer).type;
  is(type, "Number", "Check 'Application.prefs.get().type' for integer pref");

  // test resetting an existing integer property
  Application.prefs.get(testdata.integer).reset();
  var val = Application.prefs.getValue(testdata.integer, 0);
  is(val, 1, "Reset existing integer property");

  // PREF: boolean browser.underline_anchors == true

  // test to see if an existing boolean property exists
  ok(Application.prefs.has(testdata.boolean), "Check existing boolean property for existence");

  // test accessing a non-existant boolean property
  var val = Application.prefs.getValue(testdata.dummy, true);
  ok(val, "Check non-existant boolean property for expected value");

  // test accessing an existing boolean property
  var val = Application.prefs.getValue(testdata.boolean, false);
  ok(val, "Check existing boolean property for expected value");

  // test manipulating an existing boolean property
  Application.prefs.setValue(testdata.boolean, false);
  var val = Application.prefs.getValue(testdata.boolean, true);
  ok(!val, "Set existing boolean property");

  // test getting the type of an existing boolean property
  var type = Application.prefs.get(testdata.boolean).type;
  is(type, "Boolean", "Check 'Application.prefs.get().type' for boolean pref");

  // test resetting an existing boolean property
  Application.prefs.get(testdata.boolean).reset();
  var val = Application.prefs.getValue(testdata.boolean, false);
  ok(val, "Reset existing string property for expected value");

  // test getting all preferences
  var allPrefs = Application.prefs.all;
  ok(allPrefs.length >= 800, "Check 'Application.prefs.all' for the right number of preferences");
  ok(allPrefs[0].name.length > 0, "Check 'Application.prefs.all' for a valid name in the starting preference");

  // test the value of the preference root
  is(Application.prefs.root, "", "Check the Application preference root");

  // test for user changed preferences
  ok(Application.prefs.get("browser.shell.checkDefaultBrowser").modified, "A single preference is marked as modified.");
  ok(!Application.prefs.get(testdata.string).modified, "A single preference is marked as not modified.");

  // test for a locked preference
  var pref = Application.prefs.get(testdata.string);
  ok(!pref.locked, "A single preference should not be locked.");

  pref.locked = true;
  ok(pref.locked, "A single preference should be locked.");

  try {
    prev.value = "test value";

    ok(false, "A locked preference could be modified.");
  } catch(e){
    ok(true, "A locked preference should not be able to be modified.");
  }

  pref.locked = false;
  ok(!pref.locked, "A single preference is unlocked.");

  // check for change event when setting a value
  waitForExplicitFinish();
  Application.prefs.events.addListener("change", onPrefChange);
  Application.prefs.setValue("fuel.fuel-test", "change event");
}

function onPrefChange(evt) {
  is(evt.data, testdata.dummy, "Check 'Application.prefs.setValue' fired a change event");
  Application.prefs.events.removeListener("change", onPrefChange);

  // We are removing the old listener after adding the new listener so we can test that
  // removing a listener does not remove all listeners
  Application.prefs.get("fuel.fuel-test").events.addListener("change", onPrefChangeDummy);
  Application.prefs.get("fuel.fuel-test").events.addListener("change", onPrefChange2);
  Application.prefs.get("fuel.fuel-test").events.removeListener("change", onPrefChangeDummy);

  Application.prefs.setValue("fuel.fuel-test", "change event2");
}

function onPrefChange2(evt) {
  is(evt.data, testdata.dummy, "Check 'Application.prefs.setValue' fired a change event for a single preference");
  Application.prefs.events.removeListener("change", onPrefChange2);

  finish();
}

function onPrefChangeDummy(evt) {
  ok(false, "onPrefChangeDummy shouldn't be invoked!");
}
