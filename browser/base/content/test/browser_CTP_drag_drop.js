/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

let gNextTest = null;
let gNewWindow = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() { Services.prefs.clearUserPref("plugins.click_to_play"); });
  Services.prefs.setBoolPref("plugins.click_to_play", true);

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

  gBrowser.removeCurrentTab();
  finish();
}
