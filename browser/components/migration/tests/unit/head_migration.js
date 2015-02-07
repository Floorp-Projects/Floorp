/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MigrationUtils",
                                  "resource:///modules/MigrationUtils.jsm");
// Initialize profile.
let gProfD = do_get_profile();

// Create a fake XULAppInfo to satisfy the eventual needs of the migrators.
let XULAppInfo = {
  // nsIXUlAppInfo
  get vendor() "Mozilla",
  get name() "XPCShell",
  get ID() "xpcshell@tests.mozilla.org",
  get version() "1",
  get appBuildID() "2007010101",
  get platformVersion() "1.0",
  get platformBuildID() "2007010101",

  // nsIXUlRuntime (partial)
  get inSafeMode() false,
  logConsoleErrors: true,
  get OS() "XPCShell",
  get XPCOMABI() "noarch-spidermonkey",
  invalidateCachesOnRestart: function () {},

  // nsIWinAppHelper
  get userCanElevate() false,

  QueryInterface: function (aIID) {
    let interfaces = [Ci.nsIXULAppInfo, Ci.nsIXULRuntime];
    if ("nsIWinAppHelper" in Ci)
      interfaces.push(Ci.nsIWinAppHelper);
    if (!interfaces.some(function (v) aIID.equals(v)))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

const CONTRACT_ID = "@mozilla.org/xre/app-info;1";
const CID = Components.ID("7685dac8-3637-4660-a544-928c5ec0e714}");

let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(CID, "XULAppInfo", CONTRACT_ID, {
  createInstance: function (aOuter, aIID) {
    if (aOuter != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(aIID);
  },
  QueryInterface: XPCOMUtils.generateQI(Ci.nsIFactory)
});
