/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

let gNextTest = null;
let gNewWindow = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
  });
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("PluginBindingAttached", handleEvent, true, true);
  gNextTest = part1;
  gBrowser.selectedBrowser.contentDocument.location = gHttpTestRoot + "plugin_test.html";
}

function handleEvent() {
  gNextTest();
}

function part1() {
  gBrowser.selectedBrowser.removeEventListener("PluginBindingAttached", handleEvent);
  ok(PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should have a click-to-play notification in the initial tab");

  gNextTest = part2;
  gNewWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
  gNewWindow.addEventListener("load", handleEvent, true);
}

function part2() {
  gNewWindow.removeEventListener("load", handleEvent);
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);
  waitForCondition(condition, part3, "Waited too long for click-to-play notification");
}

function part3() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser), "Should have a click-to-play notification in the tab in the new window");
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should not have a click-to-play notification in the old window now");

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.swapBrowsersAndCloseOther(gBrowser.selectedTab, gNewWindow.gBrowser.selectedTab);
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  waitForCondition(condition, part4, "Waited too long for click-to-play notification");
}

function part4() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should have a click-to-play notification in the initial tab again");

  gBrowser.selectedBrowser.addEventListener("PluginBindingAttached", handleEvent, true, true);
  gNextTest = part5;
  gBrowser.selectedBrowser.contentDocument.location = gHttpTestRoot + "plugin_test.html";
}

function part5() {
  gBrowser.selectedBrowser.removeEventListener("PluginBindingAttached", handleEvent);
  ok(PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should have a click-to-play notification in the initial tab");

  gNewWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
  waitForFocus(part6, gNewWindow);
}

function part6() {
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);
  waitForCondition(condition, part7, "Waited too long for click-to-play notification");
}

function part7() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser), "Should have a click-to-play notification in the tab in the new window");
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should not have a click-to-play notification in the old window now");

  let plugin = gNewWindow.gBrowser.selectedBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "plugin should not be activated");

  EventUtils.synthesizeMouseAtCenter(plugin, {}, gNewWindow.gBrowser.selectedBrowser.contentWindow);
  let condition = function() !PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser).dismissed && gNewWindow.PopupNotifications.panel.firstChild;
  waitForCondition(condition, part8, "waited too long for plugin to activate");
}

function part8() {
  // Click the activate button on doorhanger to make sure it works
  gNewWindow.PopupNotifications.panel.firstChild._primaryButton.click();

  let plugin = gNewWindow.gBrowser.selectedBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "plugin should be activated now");

  gNewWindow.close();
  finish();
}
