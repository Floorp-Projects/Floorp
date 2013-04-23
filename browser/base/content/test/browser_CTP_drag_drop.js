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

  gBrowser.selectedBrowser.addEventListener("PluginBindingAttached", handleEvent, true, true);
  gNextTest = part5;
  gBrowser.selectedBrowser.contentDocument.location = gHttpTestRoot + "plugin_test.html";
}

function part5() {
  gBrowser.selectedBrowser.removeEventListener("PluginBindingAttached", handleEvent);
  ok(PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should have a click-to-play notification in the initial tab");

  gNextTest = part6;
  gNewWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
  gNewWindow.addEventListener("load", handleEvent, true);
}

function part6() {
  gNewWindow.removeEventListener("load", handleEvent);
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
  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, part8, "waited too long for plugin to activate");
}

function part8() {
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser), "Should not have a click-to-play notification in the tab in the new window now");
  let plugin = gNewWindow.gBrowser.selectedBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "plugin should be activated now");

  gNewWindow.close();
  gBrowser.selectedTab = gBrowser.addTab();
  gNextTest = part9;
  gBrowser.selectedBrowser.addEventListener("PluginBindingAttached", handleEvent, true, true);
  // This test page contains an "invisible" plugin. It doesn't script it,
  // but when we do later in this test, it will trigger the popup notification.
  gBrowser.selectedBrowser.contentDocument.location = gHttpTestRoot + "plugin_test_noScriptNoPopup.html";
}

function part9() {
  gBrowser.selectedBrowser.removeEventListener("PluginBindingAttached", handleEvent);
  ok(PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should have a click-to-play notification in the initial tab");

  gNextTest = part10;
  gNewWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
  gNewWindow.addEventListener("load", handleEvent, true);
}

function part10() {
  gNewWindow.removeEventListener("load", handleEvent);
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);
  waitForCondition(condition, part11, "Waited too long for click-to-play notification");
}

function part11() {
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should not have a click-to-play notification in the old window now");
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);
  ok(notification, "Should have a click-to-play notification in the tab in the new window");
  // we have to actually show the panel to get the bindings to instantiate
  notification.options.eventCallback = part12;
  // this scripts the plugin, triggering the popup notification
  try {
    gNewWindow.gBrowser.selectedBrowser.contentDocument.getElementById("test").wrappedJSObject.getObjectValue();
  } catch (e) {}
}

function part12() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);
  notification.options.eventCallback = null;
  let centerAction = null;
  for (let action of notification.options.centerActions) {
    if (action.message == "Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "Found center action for the Test plugin");

  let centerItem = null;
  for (let item of centerAction.popupnotification.childNodes) {
    if (item.action == centerAction) {
      centerItem = item;
      break;
    }
  }
  ok(centerItem, "Found center item for the Test plugin");

  // "click" the button to activate the Test plugin
  centerItem.runCallback.apply(centerItem);

  let plugin = gNewWindow.gBrowser.selectedBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, part13, "Waited too long for plugin to activate via center action");
}

function part13() {
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser), "Should not have a click-to-play notification in the tab in the new window");
  let plugin = gNewWindow.gBrowser.selectedBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Plugin should be activated via center action");

  gNewWindow.close();
  finish();
}
