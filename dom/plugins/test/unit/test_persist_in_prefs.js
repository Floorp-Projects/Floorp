/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

Components.utils.import("resource://gre/modules/Services.jsm");

// Plugin registry uses different field delimeters on different platforms
var DELIM = mozinfo.os == "win" ? "|" : ":";

var gProfD = do_get_profile_startup();

// Writes out some plugin registry to the profile
function write_registry(version, info) {
  let runtime = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime);

  let header = "Generated File. Do not edit.\n\n";
  header += "[HEADER]\n";
  header += "Version" + DELIM + version + DELIM + "$\n";
  header += "Arch" + DELIM + runtime.XPCOMABI + DELIM + "$\n";
  header += "\n";
  header += "[PLUGINS]\n";

  let registry = gProfD.clone();
  registry.append("pluginreg.dat");
  let foStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                           .createInstance(Components.interfaces.nsIFileOutputStream);
  // write, create, truncate
  foStream.init(registry, 0x02 | 0x08 | 0x20, 0666, 0);

  let charset = "UTF-8"; // Can be any character encoding name that Mozilla supports
  let os = Cc["@mozilla.org/intl/converter-output-stream;1"].
           createInstance(Ci.nsIConverterOutputStream);
  os.init(foStream, charset, 0, 0x0000);

  os.writeString(header);
  os.writeString(info);
  os.close();
}

function run_test() {
  do_check_true(gIsWindows || gIsOSX || gIsLinux);

  let file = get_test_plugin_no_symlink();
  if (!file)
    do_throw("Plugin library not found");

  const pluginDir = file.parent;
  const tempDir = do_get_tempdir();
  const suffix = get_platform_specific_plugin_suffix();
  const pluginName = file.leafName.substring(0, file.leafName.length - suffix.length).toLowerCase();
  const pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  const statePref = "plugin.state." + pluginName;

  // write plugin registry data
  let registry = "";

  registry += file.leafName + DELIM + "$\n";
  registry += file.path + DELIM + "$\n";
  registry += "1.0.0.0" + DELIM + "$\n";
  registry += file.lastModifiedTime + DELIM + "0" + DELIM + "0" + DELIM + "$\n";
  registry += "Plug-in for testing purposes." + DELIM + "$\n";
  registry += "Test Plug-in" + DELIM + "$\n";
  registry += "1\n";
  registry += "0" + DELIM + "application/x-test" + DELIM + "Test mimetype" + DELIM + "tst" + DELIM + "$\n";

  registry += "\n";
  registry += "[INVALID]\n";
  registry += "\n";
  write_registry("0.15", registry);

  // Initialise profile folder
  do_get_profile();

  let plugin = get_test_plugintag();
  if (!plugin)
    do_throw("Plugin tag not found");

  // check that the expected plugin state was loaded correctly from the registry
  do_check_true(plugin.disabled);
  do_check_false(plugin.clicktoplay);
  // ... and imported into prefs, with 0 being the disabled state
  do_check_eq(0, Services.prefs.getIntPref(statePref));

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
    do_check_false(plugin == null);
    do_check_true(plugin.disabled);
    do_check_false(plugin.clicktoplay);
  });

  // check that the state persists even if the plugin is not always present
  copy.moveTo(tempDir, null);
  pluginHost.reloadPlugins(false);
  copy.moveTo(pluginDir, null);
  pluginHost.reloadPlugins(false);

  plugin = get_test_plugintag();
  do_check_false(plugin == null);
  do_check_true(plugin.disabled);
  do_check_false(plugin.clicktoplay);

  // clean up
  Services.prefs.clearUserPref(statePref);
  Services.prefs.clearUserPref("plugin.importedState");
  copy.remove(true);
  file.moveTo(pluginDir, null);
}
