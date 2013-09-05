function paintCountIs(plugin, expected, msg) {
  var count = plugin.getPaintCount();
  var realExpected = expected;
  var isAsync = SimpleTest.testPluginIsOOP();
  if (isAsync) {
    ++realExpected; // extra paint at startup for all async-rendering plugins
  } else {
    try {
      if (SpecialPowers.Cc["@mozilla.org/gfx/info;1"].getService(SpecialPowers.Ci.nsIGfxInfo).D2DEnabled) {
        realExpected *= 2;
      }
    } catch (e) {}
  }
  ok(realExpected == count, msg + " (expected " + expected +
     " independent paints, expected " + realExpected + " logged paints, got " +
     count + " actual paints)");
}

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

// call this to set the test plugin(s) initially expected enabled state.
// it will automatically be reset to it's previous value after the test
// ends
function setTestPluginEnabledState(newEnabledState, pluginName) {
  var plugin = getTestPlugin(pluginName);
  var oldEnabledState = plugin.enabledState;
  plugin.enabledState = newEnabledState;
  SimpleTest.registerCleanupFunction(function() {
    getTestPlugin(pluginName).enabledState = oldEnabledState;
  });
}
