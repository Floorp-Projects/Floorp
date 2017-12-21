Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  allow_all_plugins();
  let pluginDefaultState = Services.prefs.getIntPref("plugin.default.state");
  // if this fails, we just have to switch around the values we're testing
  Assert.notEqual(pluginDefaultState, Ci.nsIPluginTag.STATE_DISABLED);
  let nonDefaultState = (pluginDefaultState != Ci.nsIPluginTag.STATE_ENABLED ?
                         Ci.nsIPluginTag.STATE_ENABLED :
                         Ci.nsIPluginTag.STATE_CLICKTOPLAY);
  let ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  let testPlugin = get_test_plugintag();
  // the test plugin should have the default enabledState
  Assert.equal(testPlugin.enabledState, pluginDefaultState);

  let secondTestPlugin = get_test_plugintag("Second Test Plug-in");
  // set an enabledState for the second test plugin
  secondTestPlugin.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
  // change what the default enabledState is
  Services.prefs.setIntPref("plugin.default.state", nonDefaultState);
  // the test plugin should follow the default (it has no individual pref yet)
  Assert.equal(testPlugin.enabledState, nonDefaultState);
  // the second test plugin should retain its preferred state
  Assert.equal(secondTestPlugin.enabledState, Ci.nsIPluginTag.STATE_DISABLED);

  // clean up
  testPlugin.enabledState = pluginDefaultState;
  secondTestPlugin.enabledState = pluginDefaultState;
  Services.prefs.clearUserPref("plugin.default.state");
  Services.prefs.clearUserPref("plugin.importedState");
}
