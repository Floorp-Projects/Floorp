var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

var gTestBrowser = null;
var gNextTest = null;
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

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
  requestLongerTimeout(2);
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_ENABLED;
    getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  });
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  prepareTest(test1, gTestRoot + "plugin_unknown.html");
}

function finishTest() {
  clearAllPluginPermissions();
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
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

// Due to layout being async, "PluginBindAttached" may trigger later.
// This wraps a function to force a layout flush, thus triggering it,
// and schedules the function execution so they're definitely executed
// afterwards.
function runAfterPluginBindingAttached(func) {
  let doc = gTestBrowser.contentDocument;
  return function() {
    let elems = doc.getElementsByTagName('embed');
    if (elems.length < 1) {
      elems = doc.getElementsByTagName('object');
    }
    elems[0].clientTop;
    executeSoon(func);
  };
}

// Tests a page with an unknown plugin in it.
function test1() {
  ok(PopupNotifications.getNotification("plugins-not-found", gTestBrowser), "Test 1, Should have displayed the missing plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 1, Should be a missing plugin list");
  ok(gTestBrowser.missingPlugins.has("application/x-unknown"), "Test 1, Should know about application/x-unknown");
  ok(!gTestBrowser.missingPlugins.has("application/x-test"), "Test 1, Should not know about application/x-test");

  var pluginNode = gTestBrowser.contentDocument.getElementById("unknown");
  ok(pluginNode, "Test 1, Found plugin in page");
  var objLoadingContent = pluginNode.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_UNSUPPORTED, "Test 1, plugin fallback type should be PLUGIN_UNSUPPORTED");

  var plugin = getTestPlugin();
  ok(plugin, "Should have a test plugin");
  plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  prepareTest(runAfterPluginBindingAttached(test2), gTestRoot + "plugin_test.html");
}

// Tests a page with a working plugin in it.
function test2() {
  ok(!PopupNotifications.getNotification("plugins-not-found", gTestBrowser), "Test 2, Should not have displayed the missing plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 2, Should not be a missing plugin list");

  var plugin = getTestPlugin();
  ok(plugin, "Should have a test plugin");
  plugin.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
  prepareTest(runAfterPluginBindingAttached(test3), gTestRoot + "plugin_test.html");
}

// Tests a page with a disabled plugin in it.
function test3() {
  ok(!PopupNotifications.getNotification("plugins-not-found", gTestBrowser), "Test 3, Should not have displayed the missing plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 3, Should not be a missing plugin list");

  new TabOpenListener("about:addons", test4, prepareTest5);

  var pluginNode = gTestBrowser.contentDocument.getElementById("test");
  ok(pluginNode, "Test 3, Found plugin in page");
  var objLoadingContent = pluginNode.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_DISABLED, "Test 3, plugin fallback type should be PLUGIN_DISABLED");
  var manageLink = gTestBrowser.contentDocument.getAnonymousElementByAttribute(pluginNode, "anonid", "managePluginsLink");
  ok(manageLink, "Test 3, found 'manage' link in plugin-problem binding");

  EventUtils.synthesizeMouseAtCenter(manageLink, {}, gTestBrowser.contentWindow);
}

function test4(tab, win) {
  is(win.wrappedJSObject.gViewController.currentViewId, "addons://list/plugin", "Test 4, Should have displayed the plugins pane");
  gBrowser.removeTab(tab);
}

function prepareTest5() {
  info("prepareTest5");
  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  setAndUpdateBlocklist(gHttpTestRoot + "blockPluginHard.xml",
    function() {
      info("prepareTest5 callback");
      prepareTest(runAfterPluginBindingAttached(test5), gTestRoot + "plugin_test.html");
  });
}

// Tests a page with a blocked plugin in it.
function test5() {
  info("test5");
  ok(!PopupNotifications.getNotification("plugins-not-found", gTestBrowser), "Test 5, Should not have displayed the missing plugin notification");
  let notification = PopupNotifications.getNotification("click-to-play-plugins");
  ok(notification, "Test 5: There should be a plugin notification for blocked plugins");
  ok(notification.dismissed, "Test 5: The plugin notification should be dismissed by default");

  notification.reshow();
  is(notification.options.centerActions.length, 1, "Test 5: Only the blocked plugin should be present in the notification");
  ok(PopupNotifications.panel.firstChild._buttonContainer.hidden, "Part 5: The blocked plugins notification should not have any buttons visible.");

  ok(!gTestBrowser.missingPlugins, "Test 5, Should not be a missing plugin list");
  var pluginNode = gTestBrowser.contentDocument.getElementById("test");
  ok(pluginNode, "Test 5, Found plugin in page");
  var objLoadingContent = pluginNode.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_BLOCKLISTED, "Test 5, plugin fallback type should be PLUGIN_BLOCKLISTED");

  prepareTest(runAfterPluginBindingAttached(test6), gTestRoot + "plugin_both.html");
}

// Tests a page with a blocked and unknown plugin in it.
function test6() {
  ok(PopupNotifications.getNotification("plugins-not-found", gTestBrowser), "Test 6, Should have displayed the missing plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 6, Should be a missing plugin list");
  ok(gTestBrowser.missingPlugins.has("application/x-unknown"), "Test 6, Should know about application/x-unknown");
  ok(!gTestBrowser.missingPlugins.has("application/x-test"), "Test 6, application/x-test should not be a missing plugin");

  prepareTest(runAfterPluginBindingAttached(test7), gTestRoot + "plugin_both2.html");
}

// Tests a page with a blocked and unknown plugin in it (alternate order to above).
function test7() {
  ok(PopupNotifications.getNotification("plugins-not-found", gTestBrowser), "Test 7, Should have displayed the missing plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 7, Should be a missing plugin list");
  ok(gTestBrowser.missingPlugins.has("application/x-unknown"), "Test 7, Should know about application/x-unknown");
  ok(!gTestBrowser.missingPlugins.has("application/x-test"), "Test 7, application/x-test should not be a missing plugin");

  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;

  setAndUpdateBlocklist(gHttpTestRoot + "blockNoPlugins.xml", function() {
    prepareTest(runAfterPluginBindingAttached(test8), gTestRoot + "plugin_test.html");
  });
}

// Tests a page with a working plugin that is click-to-play
function test8() {
  ok(!PopupNotifications.getNotification("plugins-not-found", gTestBrowser), "Test 8, Should not have displayed the missing plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 8, Should not be a missing plugin list");
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 8, Should have a click-to-play notification");

  var pluginNode = gTestBrowser.contentDocument.getElementById("test");
  ok(pluginNode, "Test 8, Found plugin in page");
  var objLoadingContent = pluginNode.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY, "Test 8, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");

  prepareTest(runAfterPluginBindingAttached(test11a), gTestRoot + "plugin_test3.html");
}

// Tests 9 & 10 removed

// Tests that the going back will reshow the notification for click-to-play plugins (part 1/4)
function test11a() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 11a, Should have a click-to-play notification");

  prepareTest(runAfterPluginBindingAttached(test11b), "about:blank");
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
  Services.obs.removeObserver(test11c, "PopupNotifications-updateNotShowing");
  var condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, test11d, "Test 11c, waited too long for click-to-play-plugin notification");
}

// Tests that the going back will reshow the notification for click-to-play plugins (part 4/4)
function test11d() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 11d, Should have a click-to-play notification");

  prepareTest(runAfterPluginBindingAttached(test12a), gHttpTestRoot + "plugin_clickToPlayAllow.html");
}

// Tests that the "Allow Always" permission works for click-to-play plugins
function test12a() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 12a, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 12a, Plugin should not be activated");

  // Simulate clicking the "Allow Always" button.
  popupNotification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test12b, "Test 12a, Waited too long for plugin to activate");
}

function test12b() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 12d, Should have a click-to-play notification");
  prepareTest(runAfterPluginBindingAttached(test12c), gHttpTestRoot + "plugin_two_types.html");
}

// Test that the "Always" permission, when set for just the Test plugin,
// does not also allow the Second Test plugin.
function test12c() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 12d, Should have a click-to-play notification");
  var test = gTestBrowser.contentDocument.getElementById("test");
  var secondtestA = gTestBrowser.contentDocument.getElementById("secondtestA");
  var secondtestB = gTestBrowser.contentDocument.getElementById("secondtestB");
  ok(test.activated, "Test 12d, Test plugin should be activated");
  ok(!secondtestA.activated, "Test 12d, Second Test plugin (A) should not be activated");
  ok(!secondtestB.activated, "Test 12d, Second Test plugin (B) should not be activated");

  clearAllPluginPermissions();
  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  prepareTest(test14, gTestRoot + "plugin_test2.html");
}

// Test 13 removed

// Tests that the plugin's "activated" property is true for working plugins with click-to-play disabled.
function test14() {
  var plugin = gTestBrowser.contentDocument.getElementById("test1");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 14, Plugin should be activated");

  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  prepareTest(runAfterPluginBindingAttached(test15), gTestRoot + "plugin_alternate_content.html");
}

// Tests that the overlay is shown instead of alternate content when
// plugins are click to play
function test15() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var doc = gTestBrowser.contentDocument;
  var mainBox = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(mainBox, "Test 15, Plugin with id=" + plugin.id + " overlay should exist");

  prepareTest(runAfterPluginBindingAttached(test17), gTestRoot + "plugin_bug749455.html");
}

// Test 16 removed

// Tests that mContentType is used for click-to-play plugins, and not the
// inspected type.
function test17() {
  var clickToPlayNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(clickToPlayNotification, "Test 17, Should have a click-to-play notification");
  var missingNotification = PopupNotifications.getNotification("missing-plugins", gTestBrowser);
  ok(!missingNotification, "Test 17, Should not have a missing plugin notification");

  setAndUpdateBlocklist(gHttpTestRoot + "blockPluginVulnerableUpdatable.xml",
  function() {
    prepareTest(runAfterPluginBindingAttached(test18a), gHttpTestRoot + "plugin_test.html");
  });
}

// Tests a vulnerable, updatable plugin
function test18a() {
  var clickToPlayNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(clickToPlayNotification, "Test 18a, Should have a click-to-play notification");
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  ok(plugin, "Test 18a, Found plugin in page");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE, "Test 18a, plugin fallback type should be PLUGIN_VULNERABLE_UPDATABLE");
  ok(!objLoadingContent.activated, "Test 18a, Plugin should not be activated");
  var overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(overlay.style.visibility != "hidden", "Test 18a, Plugin overlay should exist, not be hidden");
  var updateLink = doc.getAnonymousElementByAttribute(plugin, "anonid", "checkForUpdatesLink");
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
  EventUtils.synthesizeMouseAtCenter(updateLink, {}, gTestBrowser.contentWindow);
}

function test18b() {
  // clicking the update link should not activate the plugin
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 18b, Plugin should not be activated");
  var overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(overlay.style.visibility != "hidden", "Test 18b, Plugin overlay should exist, not be hidden");

  setAndUpdateBlocklist(gHttpTestRoot + "blockPluginVulnerableNoUpdate.xml",
  function() {
    prepareTest(runAfterPluginBindingAttached(test18c), gHttpTestRoot + "plugin_test.html");
  });
}

// Tests a vulnerable plugin with no update
function test18c() {
  var clickToPlayNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(clickToPlayNotification, "Test 18c, Should have a click-to-play notification");
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  ok(plugin, "Test 18c, Found plugin in page");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE, "Test 18c, plugin fallback type should be PLUGIN_VULNERABLE_NO_UPDATE");
  ok(!objLoadingContent.activated, "Test 18c, Plugin should not be activated");
  var overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(overlay.style.visibility != "hidden", "Test 18c, Plugin overlay should exist, not be hidden");
  var updateLink = doc.getAnonymousElementByAttribute(plugin, "anonid", "checkForUpdatesLink");
  ok(updateLink.style.display != "block", "Test 18c, Plugin should not have an update link");

  // check that click "Always allow" works with blocklisted plugins
  clickToPlayNotification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test18d, "Test 18d, Waited too long for plugin to activate");
}

// continue testing "Always allow"
function test18d() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 18d, Plugin should be activated");

  prepareTest(test18e, gHttpTestRoot + "plugin_test.html");
}

// continue testing "Always allow"
function test18e() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 18e, Plugin should be activated");

  clearAllPluginPermissions();
  prepareTest(runAfterPluginBindingAttached(test18f), gHttpTestRoot + "plugin_test.html");
}

// clicking the in-content overlay of a vulnerable plugin should bring
// up the notification and not directly activate the plugin
function test18f() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 18f, Should have a click-to-play notification");
  ok(notification.dismissed, "Test 18f, notification should start dismissed");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 18f, Plugin should not be activated");

  // XXXBAD: this code doesn't do what you think it does! it is actually
  // observing the "removed" event of the old notification, since we create
  // a *new* one when the plugin is clicked.
  notification.options.eventCallback = function() { executeSoon(test18g); };
  EventUtils.synthesizeMouseAtCenter(plugin, {}, gTestBrowser.contentWindow);
}

function test18g() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 18g, Should have a click-to-play notification");
  ok(!notification.dismissed, "Test 18g, notification should be open");
  notification.options.eventCallback = null;
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 18g, Plugin should not be activated");

  setAndUpdateBlocklist(gHttpTestRoot + "blockNoPlugins.xml",
  function() {
    resetBlocklist();
    prepareTest(runAfterPluginBindingAttached(test19a), gTestRoot + "plugin_test.html");
  });
}

// Tests that clicking the icon of the overlay activates the doorhanger
function test19a() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 19a, Plugin should not be activated");
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed, "Test 19a, Doorhanger should start out dismissed");

  var icon = doc.getAnonymousElementByAttribute(plugin, "class", "icon");
  EventUtils.synthesizeMouseAtCenter(icon, {}, gTestBrowser.contentWindow);
  let condition = function() !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed;
  waitForCondition(condition, test19b, "Test 19a, Waited too long for doorhanger to activate");
}

function test19b() {
  prepareTest(runAfterPluginBindingAttached(test19c), gTestRoot + "plugin_test.html");
}

// Tests that clicking the text of the overlay activates the plugin
function test19c() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 19c, Plugin should not be activated");

  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed, "Test 19c, Doorhanger should start out dismissed");

  var text = doc.getAnonymousElementByAttribute(plugin, "class", "msg msgClickToPlay");
  EventUtils.synthesizeMouseAtCenter(text, {}, gTestBrowser.contentWindow);
  let condition = function() !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed;
  waitForCondition(condition, test19d, "Test 19c, Waited too long for doorhanger to activate");
}

function test19d() {
  prepareTest(runAfterPluginBindingAttached(test19e), gTestRoot + "plugin_test.html");
}

// Tests that clicking the box of the overlay activates the doorhanger
// (just to be thorough)
function test19e() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 19e, Plugin should not be activated");

  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed, "Test 19e, Doorhanger should start out dismissed");

  EventUtils.synthesizeMouse(plugin, 50, 50, {}, gTestBrowser.contentWindow);
  let condition = function() !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed;
  waitForCondition(condition, test19f, "Test 19e, Waited too long for plugin to activate");
}

function test19f() {
  prepareTest(runAfterPluginBindingAttached(test20a), gTestRoot + "plugin_hidden_to_visible.html");
}

// Tests that a plugin in a div that goes from style="display: none" to
// "display: block" can be clicked to activate.
function test20a() {
  var clickToPlayNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!clickToPlayNotification, "Test 20a, Should not have a click-to-play notification");
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("plugin");
  var mainBox = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
  ok(mainBox, "Test 20a, plugin overlay should not be null");
  var pluginRect = mainBox.getBoundingClientRect();
  ok(pluginRect.width == 0, "Test 20a, plugin should have an overlay with 0px width");
  ok(pluginRect.height == 0, "Test 20a, plugin should have an overlay with 0px height");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 20a, plugin should not be activated");
  var div = doc.getElementById("container");
  ok(div.style.display == "none", "Test 20a, container div should be display: none");

  div.style.display = "block";
  var condition = function() {
    var pluginRect = mainBox.getBoundingClientRect();
    return (pluginRect.width == 200);
  }
  waitForCondition(condition, test20b, "Test 20a, Waited too long for plugin to become visible");
}

function test20b() {
  var clickToPlayNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(clickToPlayNotification, "Test 20b, Should now have a click-to-play notification");
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("plugin");
  var pluginRect = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox").getBoundingClientRect();
  ok(pluginRect.width == 200, "Test 20b, plugin should have an overlay with 200px width");
  ok(pluginRect.height == 200, "Test 20b, plugin should have an overlay with 200px height");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 20b, plugin should not be activated");

  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed, "Test 20b, Doorhanger should start out dismissed");

  EventUtils.synthesizeMouseAtCenter(plugin, {}, gTestBrowser.contentWindow);
  let condition = function() !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed;
  waitForCondition(condition, test20c, "Test 20b, Waited too long for plugin to activate");
}

function test20c() {
  PopupNotifications.panel.firstChild._primaryButton.click();
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("plugin");
  let condition = function() plugin.activated;
  waitForCondition(condition, test20d, "Test 20c", "Waiting for plugin to activate");
}

function test20d() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("plugin");
  var pluginRect = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox").getBoundingClientRect();
  ok(pluginRect.width == 0, "Test 20d, plugin should have click-to-play overlay with zero width");
  ok(pluginRect.height == 0, "Test 20d, plugin should have click-to-play overlay with zero height");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 20d, plugin should be activated");

  clearAllPluginPermissions();

  prepareTest(runAfterPluginBindingAttached(test21a), gTestRoot + "plugin_two_types.html");
}

// Test having multiple different types of plugin on one page
function test21a() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 21a, Should have a click-to-play notification");

  var doc = gTestBrowser.contentDocument;
  var ids = ["test", "secondtestA", "secondtestB"];
  for (var id of ids) {
    var plugin = doc.getElementById(id);
    var rect = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox").getBoundingClientRect();
    ok(rect.width == 200, "Test 21a, Plugin with id=" + plugin.id + " overlay rect should have 200px width before being clicked");
    ok(rect.height == 200, "Test 21a, Plugin with id=" + plugin.id + " overlay rect should have 200px height before being clicked");
    var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(!objLoadingContent.activated, "Test 21a, Plugin with id=" + plugin.id + " should not be activated");
  }

  // we have to actually show the panel to get the bindings to instantiate
  notification.reshow();
  is(notification.options.centerActions.length, 2, "Test 21a, Should have two types of plugin in the notification");

  var centerAction = null;
  for (var action of notification.options.centerActions) {
    if (action.pluginName == "Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "Test 21b, found center action for the Test plugin");

  var centerItem = null;
  for (var item of PopupNotifications.panel.firstChild.childNodes) {
    is(item.value, "block", "Test 21b, all plugins should start out blocked");
    if (item.action == centerAction) {
      centerItem = item;
      break;
    }
  }
  ok(centerItem, "Test 21b, found center item for the Test plugin");

  // "click" the button to activate the Test plugin
  centerItem.value = "allownow";
  PopupNotifications.panel.firstChild._primaryButton.click();

  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test21c, "Test 21b, Waited too long for plugin to activate");
}

function test21c() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 21c, Should have a click-to-play notification");

  notification.reshow();
  ok(notification.options.centerActions.length == 2, "Test 21c, Should have one type of plugin in the notification");

  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var rect = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox").getBoundingClientRect();
  ok(rect.width == 0, "Test 21c, Plugin with id=" + plugin.id + " overlay rect should have 0px width after being clicked");
  ok(rect.height == 0, "Test 21c, Plugin with id=" + plugin.id + " overlay rect should have 0px height after being clicked");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 21c, Plugin with id=" + plugin.id + " should be activated");

  var ids = ["secondtestA", "secondtestB"];
  for (var id of ids) {
    var plugin = doc.getElementById(id);
    var rect = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox").getBoundingClientRect();
    ok(rect.width == 200, "Test 21c, Plugin with id=" + plugin.id + " overlay rect should have 200px width before being clicked");
    ok(rect.height == 200, "Test 21c, Plugin with id=" + plugin.id + " overlay rect should have 200px height before being clicked");
    var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(!objLoadingContent.activated, "Test 21c, Plugin with id=" + plugin.id + " should not be activated");
  }

  var centerAction = null;
  for (var action of notification.options.centerActions) {
    if (action.pluginName == "Second Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "Test 21d, found center action for the Second Test plugin");

  var centerItem = null;
  for (var item of PopupNotifications.panel.firstChild.childNodes) {
    if (item.action == centerAction) {
      is(item.value, "block", "Test 21d, test plugin 2 should start blocked");
      centerItem = item;
      break;
    }
    else {
      is(item.value, "allownow", "Test 21d, test plugin should be enabled");
    }
  }
  ok(centerItem, "Test 21d, found center item for the Second Test plugin");

  // "click" the button to activate the Second Test plugins
  centerItem.value = "allownow";
  PopupNotifications.panel.firstChild._primaryButton.click();

  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("secondtestA");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test21e, "Test 21d, Waited too long for plugin to activate");
}

function test21e() {
  var doc = gTestBrowser.contentDocument;
  var ids = ["test", "secondtestA", "secondtestB"];
  for (var id of ids) {
    var plugin = doc.getElementById(id);
    var rect = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox").getBoundingClientRect();
    ok(rect.width == 0, "Test 21e, Plugin with id=" + plugin.id + " overlay rect should have 0px width after being clicked");
    ok(rect.height == 0, "Test 21e, Plugin with id=" + plugin.id + " overlay rect should have 0px height after being clicked");
    var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(objLoadingContent.activated, "Test 21e, Plugin with id=" + plugin.id + " should be activated");
  }

  getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;

  clearAllPluginPermissions();

  prepareTest(runAfterPluginBindingAttached(test22), gTestRoot + "plugin_test.html");
}

// Tests that a click-to-play plugin retains its activated state upon reloading
function test22() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 22, Should have a click-to-play notification");

  // Plugin should start as CTP
  var pluginNode = gTestBrowser.contentDocument.getElementById("test");
  ok(pluginNode, "Test 22, Found plugin in page");
  var objLoadingContent = pluginNode.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY, "Test 22, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");

  // Activate
  objLoadingContent.playPlugin();
  is(objLoadingContent.displayedType, Ci.nsIObjectLoadingContent.TYPE_PLUGIN, "Test 22, plugin should have started");
  ok(pluginNode.activated, "Test 22, plugin should be activated");

  // Reload plugin
  var oldVal = pluginNode.getObjectValue();
  pluginNode.src = pluginNode.src;
  is(objLoadingContent.displayedType, Ci.nsIObjectLoadingContent.TYPE_PLUGIN, "Test 22, Plugin should have retained activated state");
  ok(pluginNode.activated, "Test 22, plugin should have remained activated");
  // Sanity, ensure that we actually reloaded the instance, since this behavior might change in the future.
  var pluginsDiffer;
  try {
    pluginNode.checkObjectValue(oldVal);
  } catch (e) {
    pluginsDiffer = true;
  }
  ok(pluginsDiffer, "Test 22, plugin should have reloaded");

  prepareTest(runAfterPluginBindingAttached(test23), gTestRoot + "plugin_test.html");
}

// Tests that a click-to-play plugin resets its activated state when changing types
function test23() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 23, Should have a click-to-play notification");

  // Plugin should start as CTP
  var pluginNode = gTestBrowser.contentDocument.getElementById("test");
  ok(pluginNode, "Test 23, Found plugin in page");
  var objLoadingContent = pluginNode.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY, "Test 23, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");

  // Activate
  objLoadingContent.playPlugin();
  is(objLoadingContent.displayedType, Ci.nsIObjectLoadingContent.TYPE_PLUGIN, "Test 23, plugin should have started");
  ok(pluginNode.activated, "Test 23, plugin should be activated");

  // Reload plugin (this may need RunSoon() in the future when plugins change state asynchronously)
  pluginNode.type = null;
  // We currently don't properly change state just on type change,
  // so rebind the plugin to tree. bug 767631
  pluginNode.parentNode.appendChild(pluginNode);
  is(objLoadingContent.displayedType, Ci.nsIObjectLoadingContent.TYPE_NULL, "Test 23, plugin should be unloaded");
  pluginNode.type = "application/x-test";
  pluginNode.parentNode.appendChild(pluginNode);
  is(objLoadingContent.displayedType, Ci.nsIObjectLoadingContent.TYPE_NULL, "Test 23, Plugin should not have activated");
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY, "Test 23, Plugin should be click-to-play");
  ok(!pluginNode.activated, "Test 23, plugin node should not be activated");

  prepareTest(runAfterPluginBindingAttached(test24a), gHttpTestRoot + "plugin_test.html");
}

// Test that "always allow"-ing a plugin will not allow it when it becomes
// blocklisted.
function test24a() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 24a, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 24a, Found plugin in page");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY, "Test 24a, Plugin should be click-to-play");
  ok(!objLoadingContent.activated, "Test 24a, plugin should not be activated");

  // simulate "always allow"
  notification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();
  prepareTest(test24b, gHttpTestRoot + "plugin_test.html");
}

// did the "always allow" work as intended?
function test24b() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 24b, Found plugin in page");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 24b, plugin should be activated");
  setAndUpdateBlocklist(gHttpTestRoot + "blockPluginVulnerableUpdatable.xml",
  function() {
    prepareTest(runAfterPluginBindingAttached(test24c), gHttpTestRoot + "plugin_test.html");
  });
}

// the plugin is now blocklisted, so it should not automatically load
function test24c() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 24c, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 24c, Found plugin in page");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE, "Test 24c, Plugin should be vulnerable/updatable");
  ok(!objLoadingContent.activated, "Test 24c, plugin should not be activated");

  // simulate "always allow"
  notification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  prepareTest(test24d, gHttpTestRoot + "plugin_test.html");
}

// We should still be able to always allow a plugin after we've seen that it's
// blocklisted.
function test24d() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 24d, Found plugin in page");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 24d, plugin should be activated");

  // this resets the vulnerable plugin permission
  setAndUpdateBlocklist(gHttpTestRoot + "blockNoPlugins.xml",
  function() {
    clearAllPluginPermissions();
    resetBlocklist();
    finishTest();
  });
}
