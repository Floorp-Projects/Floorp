/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 tw=78 expandtab :
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
 * The Original Code is Chrome Registration Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

let manifests = [
  do_get_file("chrome/test/unit/data/test_data_protocol_registration.manifest"),
];
registerManifests(manifests);

let XULAppInfoFactory = {
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
