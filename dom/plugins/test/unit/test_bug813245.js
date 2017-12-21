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

  var header = "Generated File. Do not edit.\n\n";
  header += "[HEADER]\n";
  header += "Version" + DELIM + version + DELIM + "$\n";
  header += "Arch" + DELIM + runtime.XPCOMABI + DELIM + "$\n";
  header += "\n";
  header += "[PLUGINS]\n";

  var registry = gProfD.clone();
  registry.append("pluginreg.dat");
  var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                           .createInstance(Components.interfaces.nsIFileOutputStream);
  // write, create, truncate
  foStream.init(registry, 0x02 | 0x08 | 0x20, 0o666, 0);

  var charset = "UTF-8"; // Can be any character encoding name that Mozilla supports
  var os = Cc["@mozilla.org/intl/converter-output-stream;1"].
           createInstance(Ci.nsIConverterOutputStream);
  os.init(foStream, charset);

  os.writeString(header);
  os.writeString(info);
  os.close();
}

function run_test() {
  allow_all_plugins();
  var plugin = get_test_plugintag();
  Assert.ok(plugin == null);

  var file = get_test_plugin();
  if (!file) {
    do_throw("Plugin library not found");
  }

  // write plugin registry data
  let registry = "";

  if (gIsOSX) {
    registry += file.leafName + DELIM + "$\n";
    registry += file.path + DELIM + "$\n";
  } else {
    registry += file.path + DELIM + "$\n";
    registry += DELIM + "$\n";
  }
  registry += "0.0.0.0" + DELIM + "$\n";
  registry += "16725225600" + DELIM + "0" + DELIM + "5" + DELIM + "$\n";
  registry += "Plug-in for testing purposes." + DELIM + "$\n";
  registry += "Test Plug-in" + DELIM + "$\n";
  registry += "999999999999999999999999999999999999999999999999|0|5|$\n";
  registry += "0" + DELIM + "application/x-test" + DELIM + "Test mimetype" +
              DELIM + "tst" + DELIM + "$\n";

  registry += "\n";
  registry += "[INVALID]\n";
  registry += "\n";
  write_registry("0.15", registry);

  // Initialise profile folder
  do_get_profile();

  var plugin = get_test_plugintag();
  if (!plugin)
    do_throw("Plugin tag not found");

  // The plugin registry should have been rejected.
  // If not, the test plugin version would be 0.0.0.0
  Assert.equal(plugin.version, "1.0.0.0");

  // Clean up
  Services.prefs.clearUserPref("plugin.importedState");
}
