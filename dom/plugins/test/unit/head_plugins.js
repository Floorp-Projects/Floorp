/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const gIsWindows = mozinfo.os == "win";
const gIsOSX = mozinfo.os == "mac";
const gIsLinux = mozinfo.os == "linux";
const gDirSvc = Services.dirsvc;

function allow_all_plugins() {
  Services.prefs.setBoolPref("plugin.load_flash_only", false);
}

// Finds the test plugin library
function get_test_plugin(secondplugin = false) {
  for (let dir of gDirSvc.get("APluginsDL", Ci.nsISimpleEnumerator)) {
    let name = get_platform_specific_plugin_name(secondplugin);
    let plugin = dir.clone();
    plugin.append(name);
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
  }
  return null;
}

// Finds the test nsIPluginTag
function get_test_plugintag(aName = "Test Plug-in") {
  var name = aName || "Test Plug-in";
  var host = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = host.getPluginTags();

  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == name) {
      return tags[i];
    }
  }
  return null;
}

// Creates a fake ProfDS directory key, copied from do_get_profile
function do_get_profile_startup() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  // the python harness sets this in the environment for us
  let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(profd);

  let dirSvc = Services.dirsvc;
  let provider = {
    getFile(prop, persistent) {
      persistent.value = true;
      if (prop == "ProfDS") {
        return file.clone();
      }
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
  };
  dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
  return file.clone();
}

function get_platform_specific_plugin_name(secondplugin = false) {
  if (secondplugin) {
    if (gIsWindows) {
      return "npsecondtest.dll";
    }
    if (gIsOSX) {
      return "SecondTest.plugin";
    }
    if (gIsLinux) {
      return "libnpsecondtest.so";
    }
  } else {
    if (gIsWindows) {
      return "nptest.dll";
    }
    if (gIsOSX) {
      return "Test.plugin";
    }
    if (gIsLinux) {
      return "libnptest.so";
    }
  }
  return null;
}

function get_platform_specific_plugin_suffix() {
  if (gIsWindows) {
    return ".dll";
  } else if (gIsOSX) {
    return ".plugin";
  } else if (gIsLinux) {
    return ".so";
  }
  return null;
}

function get_test_plugin_no_symlink() {
  let dirSvc = Services.dirsvc;
  for (let dir of dirSvc.get("APluginsDL", Ci.nsISimpleEnumerator)) {
    let plugin = dir.clone();
    plugin.append(get_platform_specific_plugin_name());
    if (plugin.exists()) {
      return plugin;
    }
  }
  return null;
}
