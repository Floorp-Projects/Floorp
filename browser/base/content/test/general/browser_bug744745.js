/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTestBrowser = null;
var gNumPluginBindingsAttached = 0;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("plugins.click_to_play");
    var plugin = getTestPlugin();
    plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
    gTestBrowser.removeEventListener("PluginBindingAttached", pluginBindingAttached, true, true);
    gBrowser.removeCurrentTab();
    window.focus();
  });

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("PluginBindingAttached", pluginBindingAttached, true, true);
  var gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_bug744745.html";
}

function pluginBindingAttached() {
  gNumPluginBindingsAttached++;

  if (gNumPluginBindingsAttached == 1) {
    var doc = gTestBrowser.contentDocument;
    var testplugin = doc.getElementById("test");
    ok(testplugin, "should have test plugin");
    var style = getComputedStyle(testplugin);
    ok('opacity' in style, "style should have opacity set");
    is(style.opacity, 1, "opacity should be 1");
    finish();
  } else {
    ok(false, "if we've gotten here, something is quite wrong");
  }
}
