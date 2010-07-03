const XULAppInfo = {
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

const XULAppInfoFactory = {
  // These two are used when we register all our factories (and unregister)
  CID: XULAPPINFO_CID,
  scheme: "XULAppInfo",
  contractID: XULAPPINFO_CONTRACTID,
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                          XULAPPINFO_CONTRACTID, XULAppInfoFactory);

registerManifests([do_get_file("data/test_abi.manifest")]);

const catman = Components.classes["@mozilla.org/categorymanager;1"].
  getService(Components.interfaces.nsICategoryManager);

function is_registered(name) {
  try {
    var d = catman.getCategoryEntry("abitest", name);
    return d == "found";
  }
  catch (e) {
    return false;
  }
}

function run_test() {
  do_check_true(is_registered("test1"));
  do_check_false(is_registered("test2"));
  do_check_true(is_registered("test3"));
  do_check_false(is_registered("test4"));
}
