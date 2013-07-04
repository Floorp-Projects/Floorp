const gTestRoot = "http://mochi.test:8888/browser/browser/base/content/test/";

let gTestBrowser = null;
let gNextTest = null;
let gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

Components.utils.import("resource://gre/modules/Services.jsm");

// Let's do the XPCNativeWrapper dance!
function addPlugin(browser, type) {
  let contentWindow = XPCNativeWrapper.unwrap(browser.contentWindow);
  contentWindow.addPlugin(type);
}

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_ENABLED;
    getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  });
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;

  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  prepareTest(testActivateAddSameTypePart1, gTestRoot + "plugin_add_dynamically.html");
}

function finishTest() {
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

// "Activate" of a given type -> plugins of that type dynamically added should
// automatically play.
function testActivateAddSameTypePart1() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "testActivateAddSameTypePart1: should not have a click-to-play notification");
  addPlugin(gTestBrowser);
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, testActivateAddSameTypePart2, "testActivateAddSameTypePart1: waited too long for click-to-play-plugin notification");
}

function testActivateAddSameTypePart2() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "testActivateAddSameTypePart2: should have a click-to-play notification");

  popupNotification.reshow();
  testActivateAddSameTypePart3();
}

function testActivateAddSameTypePart3() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  let centerAction = null;
  for (let action of popupNotification.options.centerActions) {
    if (action.pluginName == "Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "testActivateAddSameTypePart3: found center action for the Test plugin");

  let centerItem = null;
  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    if (item.action && item.action == centerAction) {
      centerItem = item;
      break;
    }
  }
  ok(centerItem, "testActivateAddSameTypePart3: found center item for the Test plugin");

  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  ok(!plugin.activated, "testActivateAddSameTypePart3: plugin should not be activated");

  // Change the state and click the ok button to activate the Test plugin
  centerItem.value = "allownow";
  PopupNotifications.panel.firstChild._primaryButton.click();

  let condition = function() plugin.activated;
  waitForCondition(condition, testActivateAddSameTypePart4, "testActivateAddSameTypePart3: waited too long for plugin to activate");
}

function testActivateAddSameTypePart4() {
  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  ok(plugin.activated, "testActivateAddSameTypePart4: plugin should be activated");

  addPlugin(gTestBrowser);
  let condition = function() {
    let embeds = gTestBrowser.contentDocument.getElementsByTagName("embed");
    let allActivated = true;
    for (let embed of embeds) {
      if (!embed.activated)
        allActivated = false;
    }
    return allActivated && embeds.length == 2;
  };
  waitForCondition(condition, testActivateAddSameTypePart5, "testActivateAddSameTypePart4: waited too long for second plugin to activate"); }

function testActivateAddSameTypePart5() {
  let embeds = gTestBrowser.contentDocument.getElementsByTagName("embed");
  for (let embed of embeds) {
    ok(embed.activated, "testActivateAddSameTypePart5: all plugins should be activated");
  }
  clearAllPluginPermissions();
  prepareTest(testActivateAddDifferentTypePart1, gTestRoot + "plugin_add_dynamically.html");
}

// "Activate" of a given type -> plugins of other types dynamically added
// should not automatically play.
function testActivateAddDifferentTypePart1() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "testActivateAddDifferentTypePart1: should not have a click-to-play notification");
  addPlugin(gTestBrowser);
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, testActivateAddDifferentTypePart2, "testActivateAddDifferentTypePart1: waited too long for click-to-play-plugin notification");
}

function testActivateAddDifferentTypePart2() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "testActivateAddDifferentTypePart2: should have a click-to-play notification");

  // we have to actually show the panel to get the bindings to instantiate
  popupNotification.reshow();
  testActivateAddDifferentTypePart3();
}

function testActivateAddDifferentTypePart3() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  is(popupNotification.options.centerActions.length, 1, "Should be one plugin action");

  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  ok(!plugin.activated, "testActivateAddDifferentTypePart3: plugin should not be activated");

  // "click" the button to activate the Test plugin
  PopupNotifications.panel.firstChild._primaryButton.click();

  let condition = function() plugin.activated;
  waitForCondition(condition, testActivateAddDifferentTypePart4, "testActivateAddDifferentTypePart3: waited too long for plugin to activate");
}

function testActivateAddDifferentTypePart4() {
  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  ok(plugin.activated, "testActivateAddDifferentTypePart4: plugin should be activated");

  addPlugin(gTestBrowser);
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, testActivateAddDifferentTypePart5, "testActivateAddDifferentTypePart5: waited too long for popup notification");
}

function testActivateAddDifferentTypePart5() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "testActivateAddDifferentTypePart5: should have popup notification");
  let embeds = gTestBrowser.contentDocument.getElementsByTagName("embed");
  for (let embed of embeds) {
    if (embed.type == "application/x-test")
      ok(embed.activated, "testActivateAddDifferentTypePart5: Test plugin should be activated");
    else if (embed.type == "application/x-second-test")
      ok(!embed.activated, "testActivateAddDifferentTypePart5: Second Test plugin should not be activated");
  }

  finishTest();
}
