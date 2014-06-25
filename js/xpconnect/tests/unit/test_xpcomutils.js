/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests the methods on XPCOMUtils.jsm.
 */

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
        do_check_eq(ci.classID, classID);
        do_check_eq(ci.flags, flags);
        do_check_eq(ci.classDescription, classDescription);
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
    do_check_eq(accessCount, 0);

    // Get the property, making sure the access count has increased.
    do_check_eq(obj.foo, TEST_VALUE);
    do_check_eq(accessCount, 1);
    do_check_true(obj.inScope);

    // Get the property once more, making sure the access count has not
    // increased.
    do_check_eq(obj.foo, TEST_VALUE);
    do_check_eq(accessCount, 1);
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
        do_check_true(prop in service);
    for (let prop in service)
        do_check_true(prop in obj.service);
    run_next_test();
});


add_test(function test_categoryRegistration()
{
  const CATEGORY_NAME = "test-cat";
  const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
  const XULAPPINFO_CID = Components.ID("{fc937916-656b-4fb3-a395-8c63569e27a8}");

  // Create a fake app entry for our category registration apps filter.
  let XULAppInfo = {
    vendor: "Mozilla",
    name: "catRegTest",
    ID: "{adb42a9a-0d19-4849-bf4d-627614ca19be}",
    version: "1",
    appBuildID: "2007010101",
    platformVersion: "",
    platformBuildID: "2007010101",
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIXULAppInfo,
      Ci.nsIXULRuntime,
    ])
  };
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

  const EXPECTED_ENTRIES = ["CatAppRegisteredComponent",
                            "CatRegisteredComponent"];

  // Check who is registered in "test-cat" category.
  let foundEntriesCount = 0;
  let catMan = Cc["@mozilla.org/categorymanager;1"].
               getService(Ci.nsICategoryManager);
  let entries = catMan.enumerateCategory(CATEGORY_NAME);
  while (entries.hasMoreElements()) {
    foundEntriesCount++;
    let entry = entries.getNext().QueryInterface(Ci.nsISupportsCString).data;
    print("Check the found category entry (" + entry + ") is expected.");  
    do_check_true(EXPECTED_ENTRIES.indexOf(entry) != -1);
  }
  print("Check there are no more or less than expected entries.");
  do_check_eq(foundEntriesCount, EXPECTED_ENTRIES.length);
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
  do_check_eq(instance,
              Cc[XPCCOMPONENT_CONTRACTID].createInstance(Ci.nsISupports));
  // Now, for sanity, check that getService is also returning the same instance.
  do_check_eq(instance,
              Cc[XPCCOMPONENT_CONTRACTID].getService(Ci.nsISupports));

  run_next_test();
});

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

function run_test()
{
  run_next_test();
}
