function paintCountIs(plugin, expected, msg) {
  var count = plugin.getPaintCount();
  var realExpected = expected;
  var isAsync = SimpleTest.testPluginIsOOP();
  if (isAsync) {
    ++realExpected; // extra paint at startup for all async-rendering plugins
  } else {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    try {
      if (Components.classes["@mozilla.org/gfx/info;1"].getService(Components.interfaces.nsIGfxInfo).D2DEnabled) {
        realExpected *= 2;
      }
    } catch (e) {}
  }
  ok(realExpected == count, msg + " (expected " + expected +
     " independent paints, expected " + realExpected + " logged paints, got " +
     count + " actual paints)");
}
