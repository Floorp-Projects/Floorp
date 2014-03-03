/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

var gTestBrowser = null;

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("plugins.click_to_play");
    gBrowser.removeCurrentTab();
    window.focus();
  });

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  let gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_bug752516.html";

  gTestBrowser.addEventListener("load", tabLoad, true);
}

function tabLoad() {
  // Due to layout being async, "PluginBindAttached" may trigger later.
  // This forces a layout flush, thus triggering it, and schedules the
  // test so it is definitely executed afterwards.
  gTestBrowser.contentDocument.getElementById('test').clientTop;
  executeSoon(actualTest);
}

function actualTest() {
  let doc = gTestBrowser.contentDocument;
  let plugin = doc.getElementById("test");
  ok(!plugin.activated, "Plugin should not be activated");
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed, "Doorhanger should not be open");

  EventUtils.synthesizeMouseAtCenter(plugin, {}, gTestBrowser.contentWindow);
  let condition = function() !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed;
  waitForCondition(condition, finish, "Waited too long for plugin doorhanger to activate");
}
