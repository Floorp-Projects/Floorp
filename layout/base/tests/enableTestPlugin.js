// this automatically sets the test plugin to be enabled (not e.g. click-to-play)
// and resets this afterwards

(function() {
  function getTestPlugin(aPluginName) {
    var ph = SpecialPowers.Cc["@mozilla.org/plugin/host;1"]
                          .getService(SpecialPowers.Ci.nsIPluginHost);
    var tags = ph.getPluginTags();
    for (var tag of tags) {
      if (tag.name == aPluginName) {
       return tag;
      }
    }

    ok(false, "Could not find plugin tag with plugin name '" + name + "'");
    return null;
  }

  var plugin = getTestPlugin("Test Plug-in");
  var oldEnabledState = plugin.enabledState;
  plugin.enabledState = SpecialPowers.Ci.nsIPluginTag.STATE_ENABLED;
  SimpleTest.registerCleanupFunction(function() {
    getTestPlugin("Test Plug-in").enabledState = oldEnabledState;
  });
})();
