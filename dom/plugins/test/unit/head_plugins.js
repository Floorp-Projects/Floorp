/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

const gIsWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
const gIsOSX = ("nsILocalFileMac" in Ci);
const gIsLinux = ("@mozilla.org/gnome-gconf-service;1" in Cc) ||
  ("@mozilla.org/gio-service;1" in Cc);

// Finds the test plugin library
function get_test_plugin() {
  var pluginEnum = gDirSvc.get("APluginsDL", Ci.nsISimpleEnumerator);
  while (pluginEnum.hasMoreElements()) {
    let dir = pluginEnum.getNext().QueryInterface(Ci.nsILocalFile);
    let plugin = dir.clone();
    // OSX plugin
    plugin.append("Test.plugin");
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
    plugin = dir.clone();
    // *nix plugin
    plugin.append("libnptest.so");
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
    // Windows plugin
    plugin = dir.clone();
    plugin.append("nptest.dll");
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
  }
  return null;
}

// Finds the test nsIPluginTag
function get_test_plugintag(aName) {
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  var name = aName || "Test Plug-in";
  var host = Cc["@mozilla.org/plugin/host;1"].
             getService(Ci.nsIPluginHost);
  var tags = host.getPluginTags();

  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == name)
      return tags[i];
  }
  return null;
}

// Creates a fake ProfDS directory key, copied from do_get_profile
function do_get_profile_startup() {
  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  // the python harness sets this in the environment for us
  let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
  let file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(profd);

  let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties);
  let provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == "ProfDS") {
        return file.clone();
      }
      throw Components.results.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Components.interfaces.nsIDirectoryServiceProvider) ||
          iid.equals(Components.interfaces.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Components.interfaces.nsIDirectoryService)
        .registerProvider(provider);
  return file.clone();
}

function get_platform_specific_plugin_name() {
  if (gIsWindows) return "nptest.dll";
  else if (gIsOSX) return "Test.plugin";
  else if (gIsLinux) return "libnptest.so";
  else return null;
}

function get_platform_specific_plugin_suffix() {
  if (gIsWindows) return ".dll";
  else if (gIsOSX) return ".plugin";
  else if (gIsLinux) return ".so";
  else return null;
}

function get_test_plugin_no_symlink() {
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"]
                .getService(Ci.nsIProperties);
  let pluginEnum = dirSvc.get("APluginsDL", Ci.nsISimpleEnumerator);
  while (pluginEnum.hasMoreElements()) {
    let dir = pluginEnum.getNext().QueryInterface(Ci.nsILocalFile);
    let plugin = dir.clone();
    plugin.append(get_platform_specific_plugin_name());
    if (plugin.exists()) {
      return plugin;
    }
  }
  return null;
}
