/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var MANIFESTS = [
  do_get_file("data/test_bug397073.manifest")
];


registerManifests(MANIFESTS);

var XULAppInfo = {
  vendor: "Mozilla",
  name: "XPCShell",
  ID: "{39885e5f-f6b4-4e2a-87e5-6259ecf79011}",
  version: "5",
  appBuildID: "2007010101",
  platformVersion: "1.9",
  platformBuildID: "2007010101",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",
  
  QueryInterface: function QueryInterface(iid) {
    if (iid.equals(Ci.nsIXULAppInfo)
     || iid.equals(Ci.nsIXULRuntime)
     || iid.equals(Ci.nsISupports))
      return this;
  
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

var XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};
var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                          XULAPPINFO_CONTRACTID, XULAppInfoFactory);

var gIOS = Cc["@mozilla.org/network/io-service;1"]
            .getService(Ci.nsIIOService);
var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                 .getService(Ci.nsIChromeRegistry);
chromeReg.checkForNewChrome();

var target = gIOS.newFileURI(do_get_file("data"));
target = target.spec + "test/test.xul";

function test_succeeded_mapping(namespace)
{
  var uri = gIOS.newURI("chrome://" + namespace + "/content/test.xul",
                        null, null);
  try {
    var result = chromeReg.convertChromeURL(uri);
    do_check_eq(result.spec, target);
  }
  catch (ex) {
    do_throw(namespace);
  }
}

function test_failed_mapping(namespace)
{
  var uri = gIOS.newURI("chrome://" + namespace + "/content/test.xul",
                        null, null);
  try {
    var result = chromeReg.convertChromeURL(uri);
    do_throw(namespace);
  }
  catch (ex) {
  }
}

function run_test()
{
  test_succeeded_mapping("test1");
  test_succeeded_mapping("test2");

  test_failed_mapping("test3");
}
