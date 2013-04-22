/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

var gTestBrowser = null;

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("plugins.click_to_play");
    let plugin = getTestPlugin();
    plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
    gBrowser.removeCurrentTab();
    window.focus();
  });

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  let plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  let gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_bug752516.html";

  gTestBrowser.addEventListener("load", tabLoad, true);
}

function tabLoad() {
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we proceed
  executeSoon(actualTest);
}

function actualTest() {
  let doc = gTestBrowser.contentDocument;
  let plugin = doc.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Plugin should not be activated");

  EventUtils.synthesizeMouseAtCenter(plugin, {}, gTestBrowser.contentWindow);
  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, finish, "Waited too long for plugin to activate");
}
