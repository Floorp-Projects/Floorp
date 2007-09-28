// The various pieces that we'll be testing
var testdata = {
  dummyid: "fuel-dummy-extension@mozilla.org",
  dummyname: "Dummy Extension",
  inspectorid: "inspector@mozilla.org",
  inspectorname: "DOM Inspector",
  missing: "fuel.fuel-test-missing",
  dummy: "fuel.fuel-test"
};
var gLastEvent = "";

function test() {
  // test to see if a non-existant extension exists
  ok(!Application.extensions.has(testdata.dummyid), "Check non-existant extension for existance");

  // test to see if an extension exists
  ok(Application.extensions.has(testdata.inspectorid), "Check extension for existance");

  var inspector = Application.extensions.get(testdata.inspectorid);
  is(inspector.id, testdata.inspectorid, "Check 'Extension.id' for known extension");
  is(inspector.name, testdata.inspectorname, "Check 'Extension.name' for known extension");
  // The known version number changes too frequently to hardcode in
  ok(inspector.version, "Check 'Extension.version' for known extension");
  ok(inspector.firstRun, "Check 'Extension.firstRun' for known extension");
  ok(inspector.enabled, "Check 'Extension.enabled' for known extension");
  
  // test to see if extension find works
  is(Application.extensions.all.length, 1, "Check a find for all extensions");
  // STORAGE TESTING
  // Make sure the we are given the same extension (cached) so things like .storage work right
  inspector.storage.set("test", "simple check");
  ok(inspector.storage.has("test"), "Checking that extension storage worked");

  var inspector2 = Application.extensions.get(testdata.inspectorid);
  is(inspector2.id, testdata.inspectorid, "Check 'Extension.id' for known extension - from cache");
  ok(inspector.storage.has("test"), "Checking that extension storage worked - from cache");
  is(inspector2.storage.get("test", "cache"), inspector.storage.get("test", "original"), "Checking that the storage of same extension is correct - from cache");

  inspector.events.addListener("disable", onGenericEvent);
  inspector.events.addListener("enable", onGenericEvent);
  inspector.events.addListener("uninstall", onGenericEvent);
  inspector.events.addListener("cancel", onGenericEvent);
  
  var extmgr = Components.classes["@mozilla.org/extensions/manager;1"]
                         .getService(Components.interfaces.nsIExtensionManager);
                         
  extmgr.disableItem(testdata.inspectorid);
  is(gLastEvent, "disable", "Checking that disable event is fired");

  // enabling after a disable will only fire a 'cancel' event
  // see - http://mxr.mozilla.org/seamonkey/source/toolkit/mozapps/extensions/src/nsExtensionManager.js.in#5216
  extmgr.enableItem(testdata.inspectorid);
  is(gLastEvent, "cancel", "Checking that enable (cancel) event is fired");

  extmgr.uninstallItem(testdata.inspectorid);
  is(gLastEvent, "uninstall", "Checking that uninstall event is fired");

  extmgr.cancelUninstallItem(testdata.inspectorid);
  is(gLastEvent, "cancel", "Checking that cancel event is fired");

  // PREF TESTING
  // Reset the install event preference, so that we can test it again later
  inspector.prefs.get("install-event-fired").reset();

  // test the value of the preference root
  is(Application.extensions.all[0].prefs.root, "extensions.inspector@mozilla.org.", "Check an extension preference root");

  // test getting non-existing values
  var itemValue = inspector.prefs.getValue(testdata.missing, "default");
  is(itemValue, "default", "Check 'Extension.prefs.getValue' for non-existing item");
  
  is(inspector.prefs.get(testdata.missing), null, "Check 'Extension.prefs.get' for non-existing item");
  
  // test setting and getting a value
  inspector.prefs.setValue(testdata.dummy, "dummy");
  itemValue = inspector.prefs.getValue(testdata.dummy, "default");
  is(itemValue, "dummy", "Check 'Extension.prefs.getValue' for existing item");

  // test for overwriting an existing value
  inspector.prefs.setValue(testdata.dummy, "smarty");
  itemValue = inspector.prefs.getValue(testdata.dummy, "default");
  is(itemValue, "smarty", "Check 'Extension.prefs.getValue' for overwritten item");
  
  // test setting and getting a value
  inspector.prefs.get(testdata.dummy).value = "dummy2";
  itemValue = inspector.prefs.get(testdata.dummy).value;
  is(itemValue, "dummy2", "Check 'Extension.prefs.get().value' for existing item");

  // test resetting a pref [since there is no default value, the pref should disappear]
  inspector.prefs.get(testdata.dummy).reset();
  var itemValue = inspector.prefs.getValue(testdata.dummy, "default");
  is(itemValue, "default", "Check 'Extension.prefs.getValue' for reset pref");
  
  // test to see if a non-existant property exists
  ok(!inspector.prefs.has(testdata.dummy), "Check non-existant property for existance");

  waitForExplicitFinish();
  inspector.prefs.events.addListener("change", onPrefChange);
  inspector.prefs.setValue("fuel.fuel-test", "change event");
}

function onGenericEvent(event) {
  gLastEvent = event.type;
}

function onPrefChange(evt) {
  var inspector3 = Application.extensions.get(testdata.inspectorid);

  is(evt.data, testdata.dummy, "Check 'Extension.prefs.set' fired a change event");
  inspector3.prefs.events.removeListener("change", onPrefChange);

  inspector3.prefs.get("fuel.fuel-test").events.addListener("change", onPrefChange2);
  inspector3.prefs.setValue("fuel.fuel-test", "change event2");
}

function onPrefChange2(evt) {
  is(evt.data, testdata.dummy, "Check 'Extension.prefs.set' fired a change event for a single preference");
  
  finish();
}
