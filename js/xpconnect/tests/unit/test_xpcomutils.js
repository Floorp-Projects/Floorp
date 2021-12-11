/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests the methods on XPCOMUtils.jsm.
 * Also on ComponentUtils.jsm. Which is deprecated.
 */

const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {ComponentUtils} = ChromeUtils.import("resource://gre/modules/ComponentUtils.jsm");
const {Preferences} = ChromeUtils.import("resource://gre/modules/Preferences.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Tests

add_test(function test_generateQI_string_names()
{
    var x = {
        QueryInterface: ChromeUtils.generateQI([
            "nsIClassInfo",
            "nsIObserver"
        ])
    };

    try {
        x.QueryInterface(Ci.nsIClassInfo);
    } catch(e) {
        do_throw("Should QI to nsIClassInfo");
    }
    try {
        x.QueryInterface(Ci.nsIObserver);
    } catch(e) {
        do_throw("Should QI to nsIObserver");
    }
    try {
        x.QueryInterface(Ci.nsIObserverService);
        do_throw("QI should not have succeeded!");
    } catch(e) {}
    run_next_test();
});

add_test(function test_defineLazyGetter()
{
    let accessCount = 0;
    let obj = {
      inScope: false
    };
    const TEST_VALUE = "test value";
    XPCOMUtils.defineLazyGetter(obj, "foo", function() {
        accessCount++;
        this.inScope = true;
        return TEST_VALUE;
    });
    Assert.equal(accessCount, 0);

    // Get the property, making sure the access count has increased.
    Assert.equal(obj.foo, TEST_VALUE);
    Assert.equal(accessCount, 1);
    Assert.ok(obj.inScope);

    // Get the property once more, making sure the access count has not
    // increased.
    Assert.equal(obj.foo, TEST_VALUE);
    Assert.equal(accessCount, 1);
    run_next_test();
});


add_test(function test_defineLazyServiceGetter()
{
    let obj = { };
    XPCOMUtils.defineLazyServiceGetter(obj, "service",
                                       "@mozilla.org/consoleservice;1",
                                       "nsIConsoleService");
    let service = Cc["@mozilla.org/consoleservice;1"].
                  getService(Ci.nsIConsoleService);

    // Check that the lazy service getter and the actual service have the same
    // properties.
    for (let prop in obj.service)
        Assert.ok(prop in service);
    for (let prop in service)
        Assert.ok(prop in obj.service);
    run_next_test();
});


add_test(function test_defineLazyPreferenceGetter()
{
    const PREF = "xpcomutils.test.pref";

    let obj = {};
    XPCOMUtils.defineLazyPreferenceGetter(obj, "pref", PREF, "defaultValue");


    equal(obj.pref, "defaultValue", "Should return the default value before pref is set");

    Preferences.set(PREF, "currentValue");


    info("Create second getter on new object");

    obj = {};
    XPCOMUtils.defineLazyPreferenceGetter(obj, "pref", PREF, "defaultValue");


    equal(obj.pref, "currentValue", "Should return the current value on initial read when pref is already set");

    Preferences.set(PREF, "newValue");

    equal(obj.pref, "newValue", "Should return new value after preference change");

    Preferences.set(PREF, "currentValue");

    equal(obj.pref, "currentValue", "Should return new value after second preference change");


    Preferences.reset(PREF);

    equal(obj.pref, "defaultValue", "Should return default value after pref is reset");

    obj = {};
    XPCOMUtils.defineLazyPreferenceGetter(obj, "pref", PREF, "a,b",
                                          null, value => value.split(","));

    deepEqual(obj.pref, ["a", "b"], "transform is applied to default value");

    Preferences.set(PREF, "x,y,z");
    deepEqual(obj.pref, ["x", "y", "z"], "transform is applied to updated value");

    Preferences.reset(PREF);
    deepEqual(obj.pref, ["a", "b"], "transform is applied to reset default");

    if (AppConstants.DEBUG) {
      // Need to use a 'real' pref so it will have a valid prefType
      obj = {};
      Assert.throws(
        () => XPCOMUtils.defineLazyPreferenceGetter(obj, "pref", "javascript.enabled", 1),
        /Default value does not match preference type/,
        "Providing a default value of a different type than the preference throws an exception"
      );
    }

    run_next_test();
});


add_test(function test_categoryRegistration()
{
  const CATEGORY_NAME = "test-cat";
  const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
  const XULAPPINFO_CID = Components.ID("{fc937916-656b-4fb3-a395-8c63569e27a8}");

  // Create a fake app entry for our category registration apps filter.
  let tmp = {};
  ChromeUtils.import("resource://testing-common/AppInfo.jsm", tmp);
  let XULAppInfo = tmp.newAppInfo({
    name: "catRegTest",
    ID: "{adb42a9a-0d19-4849-bf4d-627614ca19be}",
    version: "1",
    platformVersion: "",
  });
  let XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Cr.NS_ERROR_NO_AGGREGATION;
      return XULAppInfo.QueryInterface(iid);
    }
  };
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(
    XULAPPINFO_CID,
    "XULAppInfo",
    XULAPPINFO_CONTRACTID,
    XULAppInfoFactory
  );

  // Load test components.
  do_load_manifest("CatRegistrationComponents.manifest");

  const expectedEntries = new Map([
    ["CatRegisteredComponent", "@unit.test.com/cat-registered-component;1"],
    ["CatAppRegisteredComponent", "@unit.test.com/cat-app-registered-component;1"],
  ]);

  // Verify the correct entries are registered in the "test-cat" category.
  for (let {entry, value} of Services.catMan.enumerateCategory(CATEGORY_NAME)) {
    ok(expectedEntries.has(entry), `${entry} is expected`);
    Assert.equal(value, expectedEntries.get(entry), "${entry} has correct value.");
    expectedEntries.delete(entry);
  }
  Assert.deepEqual(
    Array.from(expectedEntries.keys()),
    [],
    "All expected entries have been deleted."
  );
  run_next_test();
});

add_test(function test_categoryBackgroundTaskRegistration()
{
  const CATEGORY_NAME = "test-cat1";

  // Note that this test should succeed whether or not MOZ_BACKGROUNDTASKS is
  // defined.  If it's defined, there's no active task so the `backgroundtask`
  // directive is processed, dropped, and always succeeds.  If it's not defined,
  // then the `backgroundtask` directive is processed and ignored.

  // Load test components.
  do_load_manifest("CatBackgroundTaskRegistrationComponents.manifest");

  let expectedEntriesList = [
    ["Cat1RegisteredComponent", "@unit.test.com/cat1-registered-component;1"],
    ["Cat1BackgroundTaskNotRegisteredComponent", "@unit.test.com/cat1-backgroundtask-notregistered-component;1"],
  ];
  if (!AppConstants.MOZ_BACKGROUNDTASKS) {
    expectedEntriesList.push(...[
      ["Cat1BackgroundTaskRegisteredComponent", "@unit.test.com/cat1-backgroundtask-registered-component;1"],
      ["Cat1BackgroundTaskAlwaysRegisteredComponent", "@unit.test.com/cat1-backgroundtask-alwaysregistered-component;1"],
    ]);
  }
  const expectedEntries = new Map(expectedEntriesList);

  // Verify the correct entries are registered in the "test-cat" category.
  for (let {entry, value} of Services.catMan.enumerateCategory(CATEGORY_NAME)) {
    ok(expectedEntries.has(entry), `${entry} is expected`);
    Assert.equal(value, expectedEntries.get(entry), "Verify that the value is correct in the expected entries.");
    expectedEntries.delete(entry);
  }
  Assert.deepEqual(
    Array.from(expectedEntries.keys()),
    [],
    "All expected entries have been deleted."
  );
  run_next_test();
});

add_test(function test_generateSingletonFactory()
{
  const XPCCOMPONENT_CONTRACTID = "@mozilla.org/singletonComponentTest;1";
  const XPCCOMPONENT_CID = Components.ID("{31031c36-5e29-4dd9-9045-333a5d719a3e}");

  function XPCComponent() {}
  XPCComponent.prototype = {
    classID: XPCCOMPONENT_CID,
    _xpcom_factory: ComponentUtils.generateSingletonFactory(XPCComponent),
    QueryInterface: ChromeUtils.generateQI([])
  };
  let NSGetFactory = ComponentUtils.generateNSGetFactory([XPCComponent]);
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(
    XPCCOMPONENT_CID,
    "XPCComponent",
    XPCCOMPONENT_CONTRACTID,
    NSGetFactory(XPCCOMPONENT_CID)
  );

  // First, try to instance the component.
  let instance = Cc[XPCCOMPONENT_CONTRACTID].createInstance(Ci.nsISupports);
  // Try again, check that it returns the same instance as before.
  Assert.equal(instance,
               Cc[XPCCOMPONENT_CONTRACTID].createInstance(Ci.nsISupports));
  // Now, for sanity, check that getService is also returning the same instance.
  Assert.equal(instance,
               Cc[XPCCOMPONENT_CONTRACTID].getService(Ci.nsISupports));

  run_next_test();
});

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

function run_test()
{
  run_next_test();
}
