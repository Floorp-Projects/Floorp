/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

// v0.9 registry field meanings are different on Mac OS X
const CWD = do_get_cwd();
function checkOS(os) {
  const nsILocalFile_ = "nsILocalFile" + os;
  return nsILocalFile_ in Components.interfaces &&
         CWD instanceof Components.interfaces[nsILocalFile_];
}
const isMac = checkOS("Mac");

// Plugin registry uses different field delimeters on different platforms
var DELIM = ":";
if ("@mozilla.org/windows-registry-key;1" in Components.classes)
  DELIM = "|";

var gProfD = do_get_profile();
var gDirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);

// Writes out some plugin registry to the profile
function write_registry(version, info) {
  var header = "Generated File. Do not edit.\n\n";
  header += "[HEADER]\n";
  header += "Version" + DELIM + version + DELIM + "$\n\n";
  header += "[PLUGINS]\n";

  var registry = gProfD.clone();
  registry.append("pluginreg.dat");
  var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                           .createInstance(Components.interfaces.nsIFileOutputStream);
  // write, create, truncate
  foStream.init(registry, 0x02 | 0x08 | 0x20, 0666, 0);

  var charset = "UTF-8"; // Can be any character encoding name that Mozilla supports
  var os = Cc["@mozilla.org/intl/converter-output-stream;1"].
           createInstance(Ci.nsIConverterOutputStream);
  os.init(foStream, charset, 0, 0x0000);

  os.writeString(header);
  os.writeString(info);
  os.close();
}

// Finds the test nsIPluginTag
function get_test_plugintag() {
  var host = Cc["@mozilla.org/plugin/host;1"].
             getService(Ci.nsIPluginHost);
  var tags = host.getPluginTags();
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in")
      return tags[i];
  }
  return null;
}

function run_test() {
  var file = get_test_plugin();
  if (!file)
    do_throw("Plugin library not found");

  // Write out a 0.9 version registry that marks the test plugin as disabled
  var registry = "";
  if (isMac) {
    registry += file.leafName + DELIM + "$\n";
    registry += file.path + DELIM + "$\n";
  } else {
    registry += file.path + DELIM + "$\n";
    registry += DELIM + "$\n";
  }
  registry += file.lastModifiedTime + DELIM + "0" + DELIM + "0" + DELIM + "$\n";
  registry += "Plug-in for testing purposes.\u2122 " + 
                "(\u0939\u093f\u0928\u094d\u0926\u0940 " + 
                "\u4e2d\u6587 " +
                "\u0627\u0644\u0639\u0631\u0628\u064a\u0629)" +
                DELIM + "$\n";
  registry += "Test Plug-in" + DELIM + "$\n";
  registry += "1\n";
  registry += "0" + DELIM + "application/x-test" + DELIM + "Test mimetype" +
              DELIM + "tst" + DELIM + "$\n";
  write_registry("0.9", registry);

  var plugin = get_test_plugintag();
  if (!plugin)
    do_throw("Plugin tag not found");

  // If the plugin was not rescanned then this version will not be correct
  do_check_eq(plugin.version, "1.0.0.0");
  do_check_eq(plugin.description,
              "Plug-in for testing purposes.\u2122 " + 
                "(\u0939\u093f\u0928\u094d\u0926\u0940 " + 
                "\u4e2d\u6587 " +
                "\u0627\u0644\u0639\u0631\u0628\u064a\u0629)");
  // If the plugin registry was not read then the plugin will not be disabled
  do_check_true(plugin.disabled);
  do_check_false(plugin.blocklisted);
}
