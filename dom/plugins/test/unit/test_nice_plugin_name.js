/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var gAppInfo = null;

function createAppInfo(ID, name, version, platformVersion="1.0") {
  let tmp = {};
  Cu.import("resource://testing-common/AppInfo.jsm", tmp);
  tmp.updateAppInfo({
    ID, name, version, platformVersion,
    crashReporter: true
  });
  gAppInfo = tmp.getAppInfo();
}

var gPluginHost = null;

function test_expected_permission_string(aPermString) {
  gPluginHost.reloadPlugins(false);
  let plugin = get_test_plugintag();
  Assert.equal(false, plugin == null);
  Assert.equal(gPluginHost.getPermissionStringForType("application/x-test"),
               aPermString);
}

function run_test() {
  allow_all_plugins();
  Assert.ok(gIsWindows || gIsOSX || gIsLinux);
  Assert.ok(!(gIsWindows && gIsOSX) && !(gIsWindows && gIsLinux) &&
            !(gIsOSX && gIsLinux));

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  gPluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

  let expectedDefaultPermissionString = null;
  if (gIsWindows) expectedDefaultPermissionString = "plugin:nptest";
  if (gIsOSX) expectedDefaultPermissionString = "plugin:test";
  if (gIsLinux) expectedDefaultPermissionString = "plugin:libnptest";
  test_expected_permission_string(expectedDefaultPermissionString);

  let suffix = get_platform_specific_plugin_suffix();
  let pluginFile = get_test_plugin_no_symlink();
  let pluginDir = pluginFile.parent;
  pluginFile.copyTo(null, "npblah235" + suffix);
  let pluginCopy = pluginDir.clone();
  pluginCopy.append("npblah235" + suffix);
  let tempDir = do_get_tempdir();
  pluginFile.moveTo(tempDir, null);
  test_expected_permission_string("plugin:npblah");

  pluginCopy.moveTo(null, "npasdf-3.2.2" + suffix);
  test_expected_permission_string("plugin:npasdf");

  pluginCopy.moveTo(null, "npasdf_##29387!{}{[][" + suffix);
  test_expected_permission_string("plugin:npasdf");

  pluginCopy.moveTo(null, "npqtplugin7" + suffix);
  test_expected_permission_string("plugin:npqtplugin");

  pluginCopy.moveTo(null, "npFoo3d" + suffix);
  test_expected_permission_string("plugin:npfoo3d");

  pluginCopy.moveTo(null, "NPSWF32_11_5_502_146" + suffix);
  test_expected_permission_string("plugin:npswf");

  pluginCopy.remove(true);
  pluginFile.moveTo(pluginDir, null);
  test_expected_permission_string(expectedDefaultPermissionString);

  // Clean up
  Services.prefs.clearUserPref("plugin.importedState");
}
