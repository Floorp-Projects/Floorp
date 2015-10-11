/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Promise.jsm");

const gIsWindows = mozinfo.os == "win";
const gIsOSX = mozinfo.os == "mac";
const gIsLinux = mozinfo.os == "linux";
const gDirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);

// Finds the test plugin library
function get_test_plugin(secondplugin=false) {
  var pluginEnum = gDirSvc.get("APluginsDL", Ci.nsISimpleEnumerator);
  while (pluginEnum.hasMoreElements()) {
    let dir = pluginEnum.getNext().QueryInterface(Ci.nsILocalFile);
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
function get_test_plugintag(aName="Test Plug-in") {
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

function get_platform_specific_plugin_name(secondplugin=false) {
  if (secondplugin) {
    if (gIsWindows) return "npsecondtest.dll";
    if (gIsOSX) return "SecondTest.plugin";
    if (gIsLinux) return "libnpsecondtest.so";
  } else {
    if (gIsWindows) return "nptest.dll";
    if (gIsOSX) return "Test.plugin";
    if (gIsLinux) return "libnptest.so";
  }
  return null;
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

var gGlobalScope = this;
function loadAddonManager() {
  let ns = {};
  Cu.import("resource://gre/modules/Services.jsm", ns);
  let head = "../../../../toolkit/mozapps/extensions/test/xpcshell/head_addons.js";
  let file = do_get_file(head);
  let uri = ns.Services.io.newFileURI(file);
  ns.Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
}

// Install addon and return a Promise<boolean> that is
// resolve with true on success, false otherwise.
function installAddon(relativePath) {
  let deferred = Promise.defer();
  let success = () => deferred.resolve(true);
  let fail = () => deferred.resolve(false);
  let listener = {
    onDownloadCancelled: fail,
    onDownloadFailed: fail,
    onInstallCancelled: fail,
    onInstallFailed: fail,
    onInstallEnded: success,
  };

  let installCallback = install => {
    install.addListener(listener);
    install.install();
  };

  let file = do_get_file(relativePath, false);
  AddonManager.getInstallForFile(file, installCallback,
                                 "application/x-xpinstall");

  return deferred.promise;
}

// Uninstall addon and return a Promise<boolean> that is
// resolve with true on success, false otherwise.
function uninstallAddon(id) {
  let deferred = Promise.defer();

  AddonManager.getAddonByID(id, addon => {
    if (!addon) {
      deferred.resolve(false);
    }

    let listener = {};
    let handler = addon => {
      if (addon.id !== id) {
        return;
      }

      AddonManager.removeAddonListener(listener);
      deferred.resolve(true);
    };

    listener.onUninstalled = handler;
    listener.onDisabled = handler;

    AddonManager.addAddonListener(listener);
    addon.uninstall();
  });

  return deferred.promise;
}

// Returns a Promise<Addon> that is resolved with
// the corresponding addon or rejected.
function getAddonByID(id) {
  let deferred = Promise.defer();
  AddonManager.getAddonByID(id, addon => deferred.resolve(addon));
  return deferred.promise;
}
