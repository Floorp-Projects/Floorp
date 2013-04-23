const gTestRoot = getRootDirectory(gTestPath);

let gTestBrowser = null;
let gNextTest = null;
let gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
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
  prepareTest(testActivateAllPart1, gTestRoot + "plugin_add_dynamically.html");
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

// "Activate All Plugins" -> plugin of any type dynamically added to the page
// should automatically play
function testActivateAllPart1() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "testActivateAllPart1: should not have a click-to-play notification");
  gTestBrowser.contentWindow.addPlugin();
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, testActivateAllPart2, "testActivateAllPart1: waited too long for click-to-play-plugin notification");
}

function testActivateAllPart2() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "testActivateAllPart2: should have a click-to-play notification");
  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "testActivateAllPart2: plugin should not be activated");
  // "click" "Activate All Plugins"
  popupNotification.mainAction.callback();
  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, testActivateAllPart3, "testActivateAllPart2: waited too long for plugin to activate");
}

function testActivateAllPart3() {
  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "testActivateAllPart3: plugin should be activated");

  gTestBrowser.contentWindow.addPlugin("application/x-second-test");
  let condition = function() {
    let embeds = gTestBrowser.contentDocument.getElementsByTagName("embed");
    for (let embed of embeds) {
      let objLoadingContent = embed.QueryInterface(Ci.nsIObjectLoadingContent);
      if (embed.type == "application/x-second-test" && objLoadingContent.activated)
        return true;
    }
    return false;
  };
  waitForCondition(condition, testActivateAllPart4, "testActivateAllPart3: waited too long for second plugin to activate");
}

function testActivateAllPart4() {
  let embeds = gTestBrowser.contentDocument.getElementsByTagName("embed");
  for (let embed of embeds) {
    let objLoadingContent = embed.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(objLoadingContent.activated, "testActivateAllPart4: all plugins should be activated");
  }

  prepareTest(testActivateAddSameTypePart1, gTestRoot + "plugin_add_dynamically.html");
}

// "Activate" of a given type -> plugins of that type dynamically added should
// automatically play.
function testActivateAddSameTypePart1() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "testActivateAddSameTypePart1: should not have a click-to-play notification");
  gTestBrowser.contentWindow.addPlugin();
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, testActivateAddSameTypePart2, "testActivateAddSameTypePart1: waited too long for click-to-play-plugin notification");
}

function testActivateAddSameTypePart2() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "testActivateAddSameTypePart2: should have a click-to-play notification");

  // we have to actually show the panel to get the bindings to instantiate
  popupNotification.options.eventCallback = testActivateAddSameTypePart3;
  popupNotification.reshow();
}

function testActivateAddSameTypePart3() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  popupNotification.options.eventCallback = null;
  let centerAction = null;
  for (let action of popupNotification.options.centerActions) {
    if (action.message == "Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "testActivateAddSameTypePart3: found center action for the Test plugin");

  let centerItem = null;
  for (let item of centerAction.popupnotification.childNodes) {
    if (item.action && item.action == centerAction) {
      centerItem = item;
      break;
    }
  }
  ok(centerItem, "testActivateAddSameTypePart3: found center item for the Test plugin");

  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "testActivateAddSameTypePart3: plugin should not be activated");

  // "click" the button to activate the Test plugin
  centerItem.runCallback.apply(centerItem);

  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, testActivateAddSameTypePart4, "testActivateAddSameTypePart3: waited too long for plugin to activate");
}

function testActivateAddSameTypePart4() {
  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "testActivateAddSameTypePart4: plugin should be activated");

  gTestBrowser.contentWindow.addPlugin();
  let condition = function() {
    let embeds = gTestBrowser.contentDocument.getElementsByTagName("embed");
    let allActivated = true;
    for (let embed of embeds) {
      let objLoadingContent = embed.QueryInterface(Ci.nsIObjectLoadingContent);
      if (!objLoadingContent.activated)
        allActivated = false;
    }
    return allActivated && embeds.length == 2;
  };
  waitForCondition(condition, testActivateAddSameTypePart5, "testActivateAddSameTypePart4: waited too long for second plugin to activate"); }

function testActivateAddSameTypePart5() {
  let embeds = gTestBrowser.contentDocument.getElementsByTagName("embed");
  for (let embed of embeds) {
    let objLoadingContent = embed.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(objLoadingContent.activated, "testActivateAddSameTypePart5: all plugins should be activated");
  }
  prepareTest(testActivateAddDifferentTypePart1, gTestRoot + "plugin_add_dynamically.html");
}

// "Activate" of a given type -> plugins of other types dynamically added
// should not automatically play.
function testActivateAddDifferentTypePart1() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "testActivateAddDifferentTypePart1: should not have a click-to-play notification");
  gTestBrowser.contentWindow.addPlugin();
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, testActivateAddDifferentTypePart2, "testActivateAddDifferentTypePart1: waited too long for click-to-play-plugin notification");
}

function testActivateAddDifferentTypePart2() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "testActivateAddDifferentTypePart2: should have a click-to-play notification");

  // we have to actually show the panel to get the bindings to instantiate
  popupNotification.options.eventCallback = testActivateAddDifferentTypePart3;
  popupNotification.reshow();
}

function testActivateAddDifferentTypePart3() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  popupNotification.options.eventCallback = null;
  let centerAction = null;
  for (let action of popupNotification.options.centerActions) {
    if (action.message == "Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "testActivateAddDifferentTypePart3: found center action for the Test plugin");

  let centerItem = null;
  for (let item of centerAction.popupnotification.childNodes) {
    if (item.action && item.action == centerAction) {
      centerItem = item;
      break;
    }
  }
  ok(centerItem, "testActivateAddDifferentTypePart3: found center item for the Test plugin");

  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "testActivateAddDifferentTypePart3: plugin should not be activated");

  // "click" the button to activate the Test plugin
  centerItem.runCallback.apply(centerItem);

  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, testActivateAddDifferentTypePart4, "testActivateAddDifferentTypePart3: waited too long for plugin to activate");
}

function testActivateAddDifferentTypePart4() {
  let plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "testActivateAddDifferentTypePart4: plugin should be activated");

  gTestBrowser.contentWindow.addPlugin("application/x-second-test");
  let condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, testActivateAddDifferentTypePart5, "testActivateAddDifferentTypePart5: waited too long for popup notification");
}

function testActivateAddDifferentTypePart5() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "testActivateAddDifferentTypePart5: should have popup notification");
  let embeds = gTestBrowser.contentDocument.getElementsByTagName("embed");
  for (let embed of embeds) {
    let objLoadingContent = embed.QueryInterface(Ci.nsIObjectLoadingContent);
    if (embed.type == "application/x-test")
      ok(objLoadingContent.activated, "testActivateAddDifferentTypePart5: Test plugin should be activated");
    else if (embed.type == "application/x-second-test")
      ok(!objLoadingContent.activated, "testActivateAddDifferentTypePart5: Second Test plugin should not be activated");
  }

  finishTest();
}
