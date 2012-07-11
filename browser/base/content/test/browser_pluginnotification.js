var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;

var gTestBrowser = null;
var gNextTest = null;
var gClickToPlayPluginActualEvents = 0;
var gClickToPlayPluginExpectedEvents = 5;

function get_test_plugin() {
  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = ph.getPluginTags();

  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in")
      return tags[i];
  }
  ok(false, "Unable to find plugin");
}

Components.utils.import("resource://gre/modules/Services.jsm");

// This listens for the next opened tab and checks it is of the right url.
// opencallback is called when the new tab is fully loaded
// closecallback is called when the tab is closed
function TabOpenListener(url, opencallback, closecallback) {
  this.url = url;
  this.opencallback = opencallback;
  this.closecallback = closecallback;

  gBrowser.tabContainer.addEventListener("TabOpen", this, false);
}

TabOpenListener.prototype = {
  url: null,
  opencallback: null,
  closecallback: null,
  tab: null,
  browser: null,

  handleEvent: function(event) {
    if (event.type == "TabOpen") {
      gBrowser.tabContainer.removeEventListener("TabOpen", this, false);
      this.tab = event.originalTarget;
      this.browser = this.tab.linkedBrowser;
      gBrowser.addEventListener("pageshow", this, false);
    } else if (event.type == "pageshow") {
      if (event.target.location.href != this.url)
        return;
      gBrowser.removeEventListener("pageshow", this, false);
      this.tab.addEventListener("TabClose", this, false);
      var url = this.browser.contentDocument.location.href;
      is(url, this.url, "Should have opened the correct tab");
      this.opencallback(this.tab, this.browser.contentWindow);
    } else if (event.type == "TabClose") {
      if (event.originalTarget != this.tab)
        return;
      this.tab.removeEventListener("TabClose", this, false);
      this.opencallback = null;
      this.tab = null;
      this.browser = null;
      // Let the window close complete
      executeSoon(this.closecallback);
      this.closecallback = null;
    }
  }
};

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() { Services.prefs.clearUserPref("plugins.click_to_play"); });
  Services.prefs.setBoolPref("plugins.click_to_play", false);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.addEventListener("PluginClickToPlay", handlePluginClickToPlay, true);
  prepareTest(test1, gTestRoot + "plugin_unknown.html");
}

function finishTest() {
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gTestBrowser.removeEventListener("PluginClickToPlay", handlePluginClickToPlay, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function handlePluginClickToPlay() {
  gClickToPlayPluginActualEvents++;
}

function pageLoad() {
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we then go on to test the states
  executeSoon(gNextTest);
}

function prepareTest(nextTest, url) {
  gNextTest = nextTest;
  gTestBrowser.contentWindow.location = url;
}

// Tests a page with an unknown plugin in it.
function test1() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 1, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 1, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 1, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 1, Should know about application/x-unknown");
  ok(!("application/x-test" in gTestBrowser.missingPlugins), "Test 1, Should not know about application/x-test");

  var plugin = get_test_plugin();
  ok(plugin, "Should have a test plugin");
  plugin.disabled = false;
  plugin.blocklisted = false;
  prepareTest(test2, gTestRoot + "plugin_test.html");
}

// Tests a page with a working plugin in it.
function test2() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 2, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 2, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 2, Should not be a missing plugin list");

  var plugin = get_test_plugin();
  ok(plugin, "Should have a test plugin");
  plugin.disabled = true;
  prepareTest(test3, gTestRoot + "plugin_test.html");
}

// Tests a page with a disabled plugin in it.
function test3() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 3, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 3, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 3, Should not be a missing plugin list");

  new TabOpenListener("about:addons", test4, prepareTest5);

  var pluginNode = gTestBrowser.contentDocument.getElementById("test");
  ok(pluginNode, "Test 3, Found plugin in page");
  var manageLink = gTestBrowser.contentDocument.getAnonymousElementByAttribute(pluginNode, "class", "managePluginsLink");
  ok(manageLink, "Test 3, found 'manage' link in plugin-problem binding");

  EventUtils.synthesizeMouse(manageLink,
                             5, 5, {}, gTestBrowser.contentWindow);
}

function test4(tab, win) {
  is(win.wrappedJSObject.gViewController.currentViewId, "addons://list/plugin", "Test 4, Should have displayed the plugins pane");
  gBrowser.removeTab(tab);
}

function prepareTest5() {
  var plugin = get_test_plugin();
  plugin.disabled = false;
  plugin.blocklisted = true;
  prepareTest(test5, gTestRoot + "plugin_test.html");
}

// Tests a page with a blocked plugin in it.
function test5() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 5, Should not have displayed the missing plugin notification");
  ok(notificationBox.getNotificationWithValue("blocked-plugins"), "Test 5, Should have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 5, Should be a missing plugin list");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 5, Should know about application/x-test");
  ok(!("application/x-unknown" in gTestBrowser.missingPlugins), "Test 5, Should not know about application/x-unknown");

  prepareTest(test6, gTestRoot + "plugin_both.html");
}

// Tests a page with a blocked and unknown plugin in it.
function test6() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 6, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 6, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 6, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 6, Should know about application/x-unknown");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 6, Should know about application/x-test");

  prepareTest(test7, gTestRoot + "plugin_both2.html");
}

// Tests a page with a blocked and unknown plugin in it (alternate order to above).
function test7() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 7, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 7, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 7, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 7, Should know about application/x-unknown");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 7, Should know about application/x-test");

  var plugin = get_test_plugin();
  plugin.disabled = false;
  plugin.blocklisted = false;
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  prepareTest(test8, gTestRoot + "plugin_test.html");
}

// Tests a page with a working plugin that is click-to-play
function test8() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 8, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 8, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 8, Should not be a missing plugin list");
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 8, Should have a click-to-play notification");

  prepareTest(test9a, gTestRoot + "plugin_test2.html");
}

// Tests that activating one click-to-play plugin will activate only that plugin (part 1/3)
function test9a() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 9a, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 9a, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 9a, Should not be a missing plugin list");
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 9a, Should have a click-to-play notification");

  var doc = gTestBrowser.contentDocument;
  var plugin1 = doc.getElementById("test1");
  var rect = doc.getAnonymousElementByAttribute(plugin1, "class", "mainBox").getBoundingClientRect();
  ok(rect.width == 200, "Test 9a, Plugin with id=" + plugin1.id + " overlay rect should have 200px width before being clicked");
  ok(rect.height == 200, "Test 9a, Plugin with id=" + plugin1.id + " overlay rect should have 200px height before being clicked");
  var objLoadingContent = plugin1.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 9a, Plugin with id=" + plugin1.id + " should not be activated");

  var plugin2 = doc.getElementById("test2");
  var rect = doc.getAnonymousElementByAttribute(plugin2, "class", "mainBox").getBoundingClientRect();
  ok(rect.width == 200, "Test 9a, Plugin with id=" + plugin2.id + " overlay rect should have 200px width before being clicked");
  ok(rect.height == 200, "Test 9a, Plugin with id=" + plugin2.id + " overlay rect should have 200px height before being clicked");
  var objLoadingContent = plugin2.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 9a, Plugin with id=" + plugin2.id + " should not be activated");

  EventUtils.synthesizeMouse(plugin1, 100, 100, { });
  var objLoadingContent = plugin1.QueryInterface(Ci.nsIObjectLoadingContent);
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test9b, "Test 9a, Waited too long for plugin to activate");
}

// Tests that activating one click-to-play plugin will activate only that plugin (part 2/3)
function test9b() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 9b, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 9b, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 9b, Should not be a missing plugin list");
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 9b, Click to play notification should not be removed now");

  var doc = gTestBrowser.contentDocument;
  var plugin1 = doc.getElementById("test1");
  var pluginRect1 = doc.getAnonymousElementByAttribute(plugin1, "class", "mainBox").getBoundingClientRect();
  ok(pluginRect1.width == 0, "Test 9b, Plugin with id=" + plugin1.id + " should have click-to-play overlay with zero width");
  ok(pluginRect1.height == 0, "Test 9b, Plugin with id=" + plugin1.id + " should have click-to-play overlay with zero height");
  var objLoadingContent = plugin1.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 9b, Plugin with id=" + plugin1.id + " should be activated");

  var plugin2 = doc.getElementById("test2");
  var pluginRect2 = doc.getAnonymousElementByAttribute(plugin2, "class", "mainBox").getBoundingClientRect();
  ok(pluginRect2.width != 0, "Test 9b, Plugin with id=" + plugin2.id + " should not have click-to-play overlay with zero width");
  ok(pluginRect2.height != 0, "Test 9b, Plugin with id=" + plugin2.id + " should not have click-to-play overlay with zero height");
  var objLoadingContent = plugin2.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 9b, Plugin with id=" + plugin2.id + " should not be activated");

  EventUtils.synthesizeMouse(plugin2, 100, 100, { });
  var objLoadingContent = plugin2.QueryInterface(Ci.nsIObjectLoadingContent);
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test9c, "Test 9b, Waited too long for plugin to activate");
}

//
// Tests that activating one click-to-play plugin will activate only that plugin (part 3/3)
function test9c() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 9c, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 9c, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 9c, Should not be a missing plugin list");
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 9c, Click to play notification should be removed now");

  var doc = gTestBrowser.contentDocument;
  var plugin1 = doc.getElementById("test1");
  var pluginRect1 = doc.getAnonymousElementByAttribute(plugin1, "class", "mainBox").getBoundingClientRect();
  ok(pluginRect1.width == 0, "Test 9c, Plugin with id=" + plugin1.id + " should have click-to-play overlay with zero width");
  ok(pluginRect1.height == 0, "Test 9c, Plugin with id=" + plugin1.id + " should have click-to-play overlay with zero height");
  var objLoadingContent = plugin1.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 9c, Plugin with id=" + plugin1.id + " should be activated");

  var plugin2 = doc.getElementById("test1");
  var pluginRect2 = doc.getAnonymousElementByAttribute(plugin2, "class", "mainBox").getBoundingClientRect();
  ok(pluginRect2.width == 0, "Test 9c, Plugin with id=" + plugin2.id + " should have click-to-play overlay with zero width");
  ok(pluginRect2.height == 0, "Test 9c, Plugin with id=" + plugin2.id + " should have click-to-play overlay with zero height");
  var objLoadingContent = plugin2.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 9c, Plugin with id=" + plugin2.id + " should be activated");

  prepareTest(test10a, gTestRoot + "plugin_test3.html");
}

// Tests that activating a hidden click-to-play plugin through the notification works (part 1/2)
function test10a() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 10a, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 10a, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 10a, Should not be a missing plugin list");
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 10a, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 10a, Plugin should not be activated");

  popupNotification.mainAction.callback();
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test10b, "Test 10a, Waited too long for plugin to activate");
}

// Tests that activating a hidden click-to-play plugin through the notification works (part 2/2)
function test10b() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 10b, Plugin should be activated");

  prepareTest(test11a, gTestRoot + "plugin_test3.html");
}

// Tests that the going back will reshow the notification for click-to-play plugins (part 1/4)
function test11a() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 11a, Should have a click-to-play notification");

  prepareTest(test11b, "about:blank");
}

// Tests that the going back will reshow the notification for click-to-play plugins (part 2/4)
function test11b() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 11b, Should not have a click-to-play notification");

  Services.obs.addObserver(test11c, "PopupNotifications-updateNotShowing", false);
  gTestBrowser.contentWindow.history.back();
}

// Tests that the going back will reshow the notification for click-to-play plugins (part 3/4)
function test11c() {
  Services.obs.removeObserver(test11c, "PopupNotifications-updateNotShowing", false);
  var condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, test11d, "Test 11c, waited too long for click-to-play-plugin notification");
}

// Tests that the going back will reshow the notification for click-to-play plugins (part 4/4)
function test11d() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 11d, Should have a click-to-play notification");
  is(gClickToPlayPluginActualEvents, gClickToPlayPluginExpectedEvents,
     "There should be a PluginClickToPlay event for each plugin that was " +
     "blocked due to the plugins.click_to_play pref");

  prepareTest(test12a, gTestRoot + "plugin_clickToPlayAllow.html");
}

// Tests that the "Allow Always" permission works for click-to-play plugins (part 1/3)
function test12a() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 12a, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 12a, Plugin should not be activated");

  // Simulate clicking the "Allow Always" button.
  popupNotification.secondaryActions[0].callback();
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test12b, "Test 12a, Waited too long for plugin to activate");
}

// Tests that the "Always" permission works for click-to-play plugins (part 2/3)
function test12b() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 12b, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 12b, Plugin should be activated");

  prepareTest(test12c, gTestRoot + "plugin_clickToPlayAllow.html");
}

// Tests that the "Always" permission works for click-to-play plugins (part 3/3)
function test12c() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 12c, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 12c, Plugin should be activated");

  Services.perms.removeAll();
  gNextTest = test13a;
  gTestBrowser.reload();
}

// Tests that the "Deny Always" permission works for click-to-play plugins (part 1/3)
function test13a() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 13a, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 13a, Plugin should not be activated");

  // Simulate clicking the "Deny Always" button.
  popupNotification.secondaryActions[1].callback();
  test13b();
}

// Tests that the "Deny Always" permission works for click-to-play plugins (part 2/3)
function test13b() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 13b, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 13b, Plugin should not be activated");
  var overlay = gTestBrowser.contentDocument.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(overlay.style.visibility == "hidden", "Test 13b, Plugin should not have visible overlay");

  gNextTest = test13c;
  gTestBrowser.reload();
}

// Tests that the "Deny Always" permission works for click-to-play plugins (part 3/3)
function test13c() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 13c, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 13c, Plugin should not be activated");
  var overlay = gTestBrowser.contentDocument.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(overlay.style.visibility == "hidden", "Test 13c, Plugin should not have visible overlay");

  Services.perms.removeAll();
  Services.prefs.setBoolPref("plugins.click_to_play", false);
  prepareTest(test14, gTestRoot + "plugin_test2.html");
}

// Tests that the plugin's "activated" property is true for working plugins with click-to-play disabled.
function test14() {
  var plugin = gTestBrowser.contentDocument.getElementById("test1");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 14, Plugin should be activated");

  var plugin = get_test_plugin();
  plugin.disabled = false;
  plugin.blocklisted = false;
  Services.perms.removeAll();
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  prepareTest(test15, gTestRoot + "plugin_alternate_content.html");
}

// Tests that the overlay is shown instead of alternate content when
// plugins are click to play
function test15() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var doc = gTestBrowser.contentDocument;
  var mainBox = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(mainBox, "Test 15, Plugin with id=" + plugin.id + " overlay should exist");

  prepareTest(test16a, gTestRoot + "plugin_bug743421.html");
}

// Tests that a plugin dynamically added to a page after one plugin is clicked
// to play (which removes the notification) gets its own notification (1/4)
function test16a() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 16a, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentWindow.addPlugin();
  var condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, test16b, "Test 16a, Waited too long for click-to-play-plugin notification");
}

// 2/4
function test16b() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 16b, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 16b, Plugin should not be activated");
  EventUtils.synthesizeMouse(plugin, 100, 100, { });
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test16c, "Test 16b, Waited too long for plugin to activate");
}

// 3/4
function test16c() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 16c, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 16c, Plugin should be activated");
  var plugin = gTestBrowser.contentWindow.addPlugin();
  var condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, test16d, "Test 16c, Waited too long for click-to-play-plugin notification");
}

// 4/4
function test16d() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 16d, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[1];
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 16d, Plugin should not be activated");

  prepareTest(test17, gTestRoot + "plugin_bug749455.html");
}

// Tests that mContentType is used for click-to-play plugins, and not the
// inspected type.
function test17() {
  var clickToPlayNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(clickToPlayNotification, "Test 17, Should have a click-to-play notification");
  var missingNotification = PopupNotifications.getNotification("missing-plugins", gTestBrowser);
  ok(!missingNotification, "Test 17, Should not have a missing plugin notification");

  registerFakeBlocklistService(Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE);
  prepareTest(test18a, gTestRoot + "plugin_test.html");
}

const Cr = Components.results;
const Cm = Components.manager;
const Cc = Components.classes;
const gReg = Cm.QueryInterface(Ci.nsIComponentRegistrar);
const gRealBlocklistServiceCID = Cc["@mozilla.org/extensions/blocklist;1"];
const gFakeBlocklistServiceCID = Components.ID("{614b68a0-3c53-4ec0-8146-28cc1e25f8a1}");
var gFactory = null;

function registerFakeBlocklistService(blockState) {

  var BlocklistService = {
    getPluginBlocklistState: function(plugin, appVersion, toolkitVersion) {
      return blockState;
    },

    classID: gFakeBlocklistServiceCID,
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIBlocklistService])
  };

  gFactory = {
    createInstance: function(outer, iid) {
      if (outer != null)
        throw Cr.NS_ERROR_NO_AGGREGATION;
      return BlocklistService.QueryInterface(iid);
    }
  };

  gReg.registerFactory(gFakeBlocklistServiceCID,
                       "Fake Blocklist Service",
                       "@mozilla.org/extensions/blocklist;1",
                       gFactory);
}

function unregisterFakeBlocklistService() {
  if (gFactory != null ) {
    gReg.unregisterFactory(gFakeBlocklistServiceCID, gFactory);
    gFactory = null;
    // This should restore the original blocklist service:
    gReg.registerFactory(gRealBlocklistServiceCID,
                         "Blocklist Service",
                         "@mozilla.org/extensions/blocklist;1",
                         null);
  }
}

// Tests a vulnerable, updatable plugin
function test18a() {
  var clickToPlayNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(clickToPlayNotification, "Test 18a, Should have a click-to-play notification");
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 18a, Plugin should not be activated");
  var overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(overlay.style.visibility != "hidden", "Test 18a, Plugin overlay should exist, not be hidden");
  var updateLink = doc.getAnonymousElementByAttribute(plugin, "class", "checkForUpdatesLink");
  ok(updateLink.style.visibility != "hidden", "Test 18a, Plugin should have an update link");

  var tabOpenListener = new TabOpenListener(Services.urlFormatter.formatURLPref("plugins.update.url"), false, false);
  tabOpenListener.handleEvent = function(event) {
    if (event.type == "TabOpen") {
      gBrowser.tabContainer.removeEventListener("TabOpen", this, false);
      this.tab = event.originalTarget;
      ok(event.target.label == this.url, "Test 18a, Update link should open up the plugin check page");
      gBrowser.removeTab(this.tab);
      test18b();
    }
  };
  EventUtils.synthesizeMouse(updateLink, 5, 5, {}, gTestBrowser.contentWindow);
}

function test18b() {
  unregisterFakeBlocklistService();
  registerFakeBlocklistService(Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE);
  prepareTest(test18c, gTestRoot + "plugin_test.html");
}

// Tests a vulnerable plugin with no update
function test18c() {
  var clickToPlayNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(clickToPlayNotification, "Test 18c, Should have a click-to-play notification");
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 18c, Plugin should not be activated");
  var overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(overlay.style.visibility != "hidden", "Test 18c, Plugin overlay should exist, not be hidden");
  var updateLink = doc.getAnonymousElementByAttribute(plugin, "class", "checkForUpdatesLink");
  ok(updateLink.style.display != "block", "Test 18c, Plugin should not have an update link");

  unregisterFakeBlocklistService();
  var plugin = get_test_plugin();
  plugin.clicktoplay = false;

  finishTest();
}
