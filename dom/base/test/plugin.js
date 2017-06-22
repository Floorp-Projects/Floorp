// Copied from /dom/plugins/test/mochitest/utils.js
function getTestPlugin(pluginName) {
  var ph = SpecialPowers.Cc["@mozilla.org/plugin/host;1"]
                                 .getService(SpecialPowers.Ci.nsIPluginHost);
  var tags = ph.getPluginTags();
  var name = pluginName || "Test Plug-in";
  for (var tag of tags) {
    if (tag.name == name) {
      return tag;
    }
  }

  ok(false, "Could not find plugin tag with plugin name '" + name + "'");
  return null;
}
// Copied from /dom/plugins/test/mochitest/utils.js
function setTestPluginEnabledState(newEnabledState, pluginName) {
  var oldEnabledState = SpecialPowers.setTestPluginEnabledState(newEnabledState, pluginName);
  if (!oldEnabledState) {
    return;
  }
  var plugin = getTestPlugin(pluginName);
  // Run a nested event loop to wait for the preference change to
  // propagate to the child. Yuck!
  SpecialPowers.Services.tm.spinEventLoopUntil(() => {
    return plugin.enabledState == newEnabledState;
  });
  SimpleTest.registerCleanupFunction(function() {
    SpecialPowers.setTestPluginEnabledState(oldEnabledState, pluginName);
  });
}
setTestPluginEnabledState(SpecialPowers.Ci.nsIPluginTag.STATE_ENABLED);
