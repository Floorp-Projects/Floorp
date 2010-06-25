/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4 et
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Necko Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu> (Original Author)
 *   Shawn Wilsher <me@shawnwilsher.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * This file tests the methods on XPCOMUtils.jsm.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;


////////////////////////////////////////////////////////////////////////////////
//// Tests

function test_generateQI_string_names()
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
}


function test_defineLazyGetter()
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
}


function test_defineLazyServiceGetter()
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
}


function test_categoryRegistration()
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
}


////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let tests = [
    test_generateQI_string_names,
    test_defineLazyGetter,
    test_defineLazyServiceGetter,
    test_categoryRegistration,
];

function run_test()
{
    tests.forEach(function(test) {
        print("Running next test: " + test.name);
        test();
    });
}
