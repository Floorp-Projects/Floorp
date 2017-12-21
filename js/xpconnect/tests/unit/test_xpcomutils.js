/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests the methods on XPCOMUtils.jsm.
 */

Components.utils.import("resource://gre/modules/Preferences.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;


////////////////////////////////////////////////////////////////////////////////
//// Tests

add_test(function test_generateQI_string_names()
{
    var x = {
        QueryInterface: XPCOMUtils.generateQI([
            Components.interfaces.nsIClassInfo,
            "nsIDOMNode"
        ])
    };

    try {
        x.QueryInterface(Components.interfaces.nsIClassInfo);
    } catch(e) {
        do_throw("Should QI to nsIClassInfo");
    }
    try {
        x.QueryInterface(Components.interfaces.nsIDOMNode);
    } catch(e) {
        do_throw("Should QI to nsIDOMNode");
    }
    try {
        x.QueryInterface(Components.interfaces.nsIDOMDocument);
        do_throw("QI should not have succeeded!");
    } catch(e) {}
    run_next_test();
});


add_test(function test_generateCI()
{
    const classID = Components.ID("562dae2e-7cff-432b-995b-3d4c03fa2b89");
    const classDescription = "generateCI test component";
    const flags = Components.interfaces.nsIClassInfo.DOM_OBJECT;
    var x = {
        QueryInterface: XPCOMUtils.generateQI([]),
        classInfo: XPCOMUtils.generateCI({classID: classID,
                                          interfaces: [],
                                          flags: flags,
                                          classDescription: classDescription})
    };

    try {
        var ci = x.QueryInterface(Components.interfaces.nsIClassInfo);
        ci = ci.QueryInterface(Components.interfaces.nsISupports);
        ci = ci.QueryInterface(Components.interfaces.nsIClassInfo);
        Assert.equal(ci.classID, classID);
        Assert.equal(ci.flags, flags);
        Assert.equal(ci.classDescription, classDescription);
    } catch(e) {
        do_throw("Classinfo for x should not be missing or broken");
    }
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


    do_print("Create second getter on new object");

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

    run_next_test();
});


add_test(function test_categoryRegistration()
{
  const CATEGORY_NAME = "test-cat";
  const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
  const XULAPPINFO_CID = Components.ID("{fc937916-656b-4fb3-a395-8c63569e27a8}");

  // Create a fake app entry for our category registration apps filter.
  let tmp = {};
  Components.utils.import("resource://testing-common/AppInfo.jsm", tmp);
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

  const EXPECTED_ENTRIES = new Map([
    ["CatRegisteredComponent", "@unit.test.com/cat-registered-component;1"],
    ["CatAppRegisteredComponent", "@unit.test.com/cat-app-registered-component;1"],
  ]);

  // Verify the correct entries are registered in the "test-cat" category.
  for (let [name, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_NAME)) {
    print("Verify that the name/value pair exists in the expected entries.");
    ok(EXPECTED_ENTRIES.has(name));
    Assert.equal(EXPECTED_ENTRIES.get(name), value);
    EXPECTED_ENTRIES.delete(name);
  }
  print("Check that all of the expected entries have been deleted.");
  Assert.equal(EXPECTED_ENTRIES.size, 0);
  run_next_test();
});

add_test(function test_generateSingletonFactory()
{
  const XPCCOMPONENT_CONTRACTID = "@mozilla.org/singletonComponentTest;1";
  const XPCCOMPONENT_CID = Components.ID("{31031c36-5e29-4dd9-9045-333a5d719a3e}");

  function XPCComponent() {}
  XPCComponent.prototype = {
    classID: XPCCOMPONENT_CID,
    _xpcom_factory: XPCOMUtils.generateSingletonFactory(XPCComponent),
    QueryInterface: XPCOMUtils.generateQI([])
  };
  let NSGetFactory = XPCOMUtils.generateNSGetFactory([XPCComponent]);
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
