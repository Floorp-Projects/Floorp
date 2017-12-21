/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

Components.utils.import("resource://gre/modules/Services.jsm");

// Plugin registry uses different field delimeters on different platforms
var DELIM = mozinfo.os == "win" ? "|" : ":";

var gProfD = do_get_profile_startup();

function run_test() {
  allow_all_plugins();

  Assert.ok(gIsWindows || gIsOSX || gIsLinux);

  let file = get_test_plugin_no_symlink();
  if (!file)
    do_throw("Plugin library not found");

  const pluginDir = file.parent;
  const tempDir = do_get_tempdir();
  const suffix = get_platform_specific_plugin_suffix();
  const pluginName = file.leafName.substring(0, file.leafName.length - suffix.length).toLowerCase();
  const pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  const statePref = "plugin.state." + pluginName;

  // Initialise profile folder
  do_get_profile();

  let plugin = get_test_plugintag();
  if (!plugin)
    do_throw("Plugin tag not found");

  plugin.enabledState = Ci.nsIPluginTag.STATE_DISABLED;

  // prepare a copy of the plugin and backup the original
  file.copyTo(null, "nptestcopy" + suffix);
  let copy = pluginDir.clone();
  copy.append("nptestcopy" + suffix);
  file.moveTo(tempDir, null);

  // test that the settings persist through a few variations of test-plugin names
  let testNames = [
    pluginName + "2",
    pluginName.toUpperCase() + "_11_5_42_2323",
    pluginName + "-5.2.7"
  ];
  testNames.forEach(function(leafName) {
    dump("Checking " + leafName + ".\n");
    copy.moveTo(null, leafName + suffix);
    pluginHost.reloadPlugins(false);
    plugin = get_test_plugintag();
    Assert.equal(false, plugin == null);
    Assert.ok(plugin.disabled);
    Assert.ok(!plugin.clicktoplay);
  });

  // check that the state persists even if the plugin is not always present
  copy.moveTo(tempDir, null);
  pluginHost.reloadPlugins(false);
  copy.moveTo(pluginDir, null);
  pluginHost.reloadPlugins(false);

  plugin = get_test_plugintag();
  Assert.equal(false, plugin == null);
  Assert.ok(plugin.disabled);
  Assert.ok(!plugin.clicktoplay);

  // clean up
  Services.prefs.clearUserPref(statePref);
  Services.prefs.clearUserPref("plugin.importedState");
  copy.remove(true);
  file.moveTo(pluginDir, null);
}
