/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 tw=78 expandtab :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var manifests = [
  do_get_file("data/test_data_protocol_registration.manifest"),
];
registerManifests(manifests);

var XULAppInfoFactory = {
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

function run_test()
{
  // Add our XULAppInfo factory
  let factories = [XULAppInfoFactory];

  // Register our factories
  for (let i = 0; i < factories.length; i++) {
    let factory = factories[i];
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
              .registerFactory(factory.CID, "test-" + factory.scheme,
                               factory.contractID, factory);
  }

  // Check for new chrome
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].
           getService(Ci.nsIChromeRegistry);
  cr.checkForNewChrome();

  // Check that our override worked
  let expectedURI = "data:application/vnd.mozilla.xul+xml,";
  let sourceURI = "chrome://good-package/content/test.xul";
  try {
    let ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    sourceURI = ios.newURI(sourceURI, null, null);
    // this throws for packages that are not registered
    let uri = cr.convertChromeURL(sourceURI).spec;

    do_check_eq(expectedURI, uri);
  }
  catch (e) {
    dump(e + "\n");
    do_throw("Should have registered our URI!");
  }

  // Unregister our factories so we do not leak
  for (let i = 0; i < factories.length; i++) {
    let factory = factories[i];
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
              .unregisterFactory(factory.CID, factory);
  }
}
