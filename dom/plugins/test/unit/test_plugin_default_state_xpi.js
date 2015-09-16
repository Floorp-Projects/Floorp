/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const ADDON_ID = "test-plugin-from-xpi@tests.mozilla.org";
const XRE_EXTENSIONS_DIR_LIST = "XREExtDL";
const NS_APP_PLUGINS_DIR_LIST = "APluginsDL";

const gPluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
const gXPCOMABI = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).XPCOMABI;
var gProfileDir = null;

function getAddonRoot(profileDir, id) {
  let dir = profileDir.clone();
  dir.append("extensions");
  Assert.ok(dir.exists(), "Extensions dir should exist: " + dir.path);
  dir.append(id);
  return dir;
}

function getTestaddonFilename() {
  let abiPart = "";
  if (gIsOSX) {
    abiPart = "_" + gXPCOMABI;
  }
  return "testaddon" + abiPart + ".xpi";
}

function run_test() {
  loadAddonManager();
  gProfileDir = do_get_profile();
  do_register_cleanup(() => shutdownManager());
  run_next_test();
}

add_task(function* test_state() {
  // Remove test so we will have only one "Test Plug-in" registered.
  // xpcshell tests have plugins in per-test profiles, so that's fine.
  let file = get_test_plugin();
  file.remove(true);
  file = get_test_plugin(true);
  file.remove(true);

  Services.prefs.setIntPref("plugin.default.state", Ci.nsIPluginTag.STATE_CLICKTOPLAY);
  Services.prefs.setIntPref("plugin.defaultXpi.state", Ci.nsIPluginTag.STATE_ENABLED);

  let success = yield installAddon(getTestaddonFilename());
  Assert.ok(success, "Should have installed addon.");
  let addonDir = getAddonRoot(gProfileDir, ADDON_ID);

  let provider = {
    classID: Components.ID("{0af6b2d7-a06c-49b7-babc-636d292b0dbb}"),
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider,
                                           Ci.nsIDirectoryServiceProvider2]),

    getFile: function (prop, persistant) {
      throw Cr.NS_ERROR_FAILURE;
    },

    getFiles: function (prop) {
      let result = [];

      switch (prop) {
      case XRE_EXTENSIONS_DIR_LIST:
        result.push(addonDir);
        break;
      case NS_APP_PLUGINS_DIR_LIST:
        let pluginDir = addonDir.clone();
        pluginDir.append("plugins");
        result.push(pluginDir);
        break;
      default:
        throw Cr.NS_ERROR_FAILURE;
      }

      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsISimpleEnumerator]),
        hasMoreElements: () => result.length > 0,
        getNext: () => result.shift(),
      };
    },
  };

  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
  dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

  // We installed a non-restartless addon, need to restart the manager.
  restartManager();
  gPluginHost.reloadPlugins();

  Assert.ok(addonDir.exists(), "Addon path should exist: " + addonDir.path);
  Assert.ok(addonDir.isDirectory(), "Addon path should be a directory: " + addonDir.path);
  let pluginDir = addonDir.clone();
  pluginDir.append("plugins");
  Assert.ok(pluginDir.exists(), "Addon plugins path should exist: " + pluginDir.path);
  Assert.ok(pluginDir.isDirectory(), "Addon plugins path should be a directory: " + pluginDir.path);

  let addon = yield getAddonByID(ADDON_ID);
  Assert.ok(!addon.appDisabled, "Addon should not be appDisabled");
  Assert.ok(addon.isActive, "Addon should be active");
  Assert.ok(addon.isCompatible, "Addon should be compatible");
  Assert.ok(!addon.userDisabled, "Addon should not be user disabled");

  let testPlugin = get_test_plugintag();
  Assert.notEqual(testPlugin, null, "Test plugin should have been found");
  Assert.equal(testPlugin.enabledState, Ci.nsIPluginTag.STATE_ENABLED, "Test plugin from addon should have state enabled");

  pluginDir.append(testPlugin.filename);
  Assert.ok(pluginDir.exists(), "Plugin file should exist in addon directory: " + pluginDir.path);

  testPlugin = get_test_plugintag("Second Test Plug-in");
  Assert.notEqual(testPlugin, null, "Second test plugin should have been found");
  Assert.equal(testPlugin.enabledState, Ci.nsIPluginTag.STATE_ENABLED, "Second test plugin from addon should have state enabled");
});
