var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

var gTestBrowser = null;
var gNextTest = null;
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

Components.utils.import("resource://gre/modules/Services.jsm");

var gPrivateWindow = null;
var gPrivateBrowser = null;

function finishTest() {
  clearAllPluginPermissions();
  gBrowser.removeCurrentTab();
  if (gPrivateWindow) {
    gPrivateWindow.close();
  }
  window.focus();
}

let createPrivateWindow = async function createPrivateWindow(url) {
  gPrivateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  ok(!!gPrivateWindow, "should have created a private window.");
  gPrivateBrowser = gPrivateWindow.getBrowser().selectedBrowser;

  BrowserTestUtils.loadURI(gPrivateBrowser, url);
  await BrowserTestUtils.browserLoaded(gPrivateBrowser);
};

add_task(async function test() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_ENABLED;
    getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  });

  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  let promise = BrowserTestUtils.browserLoaded(gTestBrowser);

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  await promise;
});

add_task(async function test1a() {
  await createPrivateWindow(gHttpTestRoot + "plugin_test.html");
});

add_task(async function test1b() {
  let popupNotification = gPrivateWindow.PopupNotifications.getNotification("click-to-play-plugins", gPrivateBrowser);
  ok(popupNotification, "Test 1b, Should have a click-to-play notification");

  await ContentTask.spawn(gPrivateBrowser, null, function() {
    let plugin = content.document.getElementById("test");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(!objLoadingContent.activated, "Test 1b, Plugin should not be activated");
  });

  // Check the button status
  let promiseShown = BrowserTestUtils.waitForEvent(gPrivateWindow.PopupNotifications.panel,
                                                   "Shown");
  popupNotification.reshow();

  await promiseShown;
  let button1 = gPrivateWindow.PopupNotifications.panel.firstChild._primaryButton;
  let button2 = gPrivateWindow.PopupNotifications.panel.firstChild._secondaryButton;
  is(button1.getAttribute("action"), "_singleActivateNow", "Test 1b, Blocked plugin in private window should have a activate now button");
  ok(button2.hidden, "Test 1b, Blocked plugin in a private window should not have a secondary button")

  gPrivateWindow.close();
  BrowserTestUtils.loadURI(gTestBrowser, gHttpTestRoot + "plugin_test.html");
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function test2a() {
  // enable test plugin on this site
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 2a, Should have a click-to-play notification");

  await ContentTask.spawn(gTestBrowser, null, function() {
    let plugin = content.document.getElementById("test");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(!objLoadingContent.activated, "Test 2a, Plugin should not be activated");
  });

  // Simulate clicking the "Allow Now" button.
  let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                   "Shown");
  popupNotification.reshow();
  await promiseShown;

  PopupNotifications.panel.firstChild._secondaryButton.click();

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let plugin = content.document.getElementById("test");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    let condition = () => objLoadingContent.activated;
    await ContentTaskUtils.waitForCondition(condition, "Test 2a, Waited too long for plugin to activate");
  });
});

add_task(async function test2c() {
  let topicObserved = TestUtils.topicObserved("PopupNotifications-updateNotShowing");
  await createPrivateWindow(gHttpTestRoot + "plugin_test.html");
  await topicObserved;

  let popupNotification = gPrivateWindow.PopupNotifications.getNotification("click-to-play-plugins", gPrivateBrowser);
  ok(popupNotification, "Test 2c, Should have a click-to-play notification");

  await ContentTask.spawn(gPrivateBrowser, null, function() {
    let plugin = content.document.getElementById("test");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(objLoadingContent.activated, "Test 2c, Plugin should be activated");
  });

  // Check the button status
  let promiseShown = BrowserTestUtils.waitForEvent(gPrivateWindow.PopupNotifications.panel,
                                                   "Shown");
  popupNotification.reshow();
  await promiseShown;
  let buttonContainer = gPrivateWindow.PopupNotifications.panel.firstChild._buttonContainer;
  ok(buttonContainer.hidden, "Test 2c, Activated plugin in a private window should not have visible buttons");

  clearAllPluginPermissions();
  gPrivateWindow.close();

  BrowserTestUtils.loadURI(gTestBrowser, gHttpTestRoot + "plugin_test.html");
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

add_task(async function test3a() {
  // enable test plugin on this site
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 3a, Should have a click-to-play notification");

  await ContentTask.spawn(gTestBrowser, null, function() {
    let plugin = content.document.getElementById("test");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(!objLoadingContent.activated, "Test 3a, Plugin should not be activated");
  });

  // Simulate clicking the "Allow Always" button.
  let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                   "Shown");
  popupNotification.reshow();
  await promiseShown;
  PopupNotifications.panel.firstChild._secondaryButton.click();

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let plugin = content.document.getElementById("test");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    let condition = () => objLoadingContent.activated;
    await ContentTaskUtils.waitForCondition(condition, "Test 3a, Waited too long for plugin to activate");
  });
});

add_task(async function test3c() {
  let topicObserved = TestUtils.topicObserved("PopupNotifications-updateNotShowing");
  await createPrivateWindow(gHttpTestRoot + "plugin_test.html");
  await topicObserved;

  let popupNotification = gPrivateWindow.PopupNotifications.getNotification("click-to-play-plugins", gPrivateBrowser);
  ok(popupNotification, "Test 3c, Should have a click-to-play notification");

  // Check the button status
  let promiseShown = BrowserTestUtils.waitForEvent(gPrivateWindow.PopupNotifications.panel,
                                                   "Shown");
  popupNotification.reshow();
  await promiseShown;
  let buttonContainer = gPrivateWindow.PopupNotifications.panel.firstChild._buttonContainer;
  ok(buttonContainer.hidden, "Test 3c, Activated plugin in a private window should not have visible buttons");

  BrowserTestUtils.loadURI(gPrivateBrowser, gHttpTestRoot + "plugin_two_types.html");
  await BrowserTestUtils.browserLoaded(gPrivateBrowser);
});

add_task(async function test3d() {
  let popupNotification = gPrivateWindow.PopupNotifications.getNotification("click-to-play-plugins", gPrivateBrowser);
  ok(popupNotification, "Test 3d, Should have a click-to-play notification");

  // Check the list item status
  let promiseShown = BrowserTestUtils.waitForEvent(gPrivateWindow.PopupNotifications.panel,
                                                   "Shown");
  popupNotification.reshow();
  await promiseShown;
  let doc = gPrivateWindow.document;
  for (let item of gPrivateWindow.PopupNotifications.panel.firstChild.childNodes) {
    let allowalways = doc.getAnonymousElementByAttribute(item, "anonid", "allowalways");
    ok(allowalways, "Test 3d, should have list item for allow always");
    let allownow = doc.getAnonymousElementByAttribute(item, "anonid", "allownow");
    ok(allownow, "Test 3d, should have list item for allow now");
    let block = doc.getAnonymousElementByAttribute(item, "anonid", "block");
    ok(block, "Test 3d, should have list item for block");

    if (item.action.pluginName === "Test") {
      is(item.value, "allowalways", "Test 3d, Plugin should bet set to 'allow always'");
      ok(!allowalways.hidden, "Test 3d, Plugin set to 'always allow' should have a visible 'always allow' action.");
      ok(allownow.hidden, "Test 3d, Plugin set to 'always allow' should have an invisible 'allow now' action.");
      ok(block.hidden, "Test 3d, Plugin set to 'always allow' should have an invisible 'block' action.");
    } else if (item.action.pluginName === "Second Test") {
      is(item.value, "block", "Test 3d, Second plugin should bet set to 'block'");
      ok(allowalways.hidden, "Test 3d, Plugin set to 'block' should have a visible 'always allow' action.");
      ok(!allownow.hidden, "Test 3d, Plugin set to 'block' should have a visible 'allow now' action.");
      ok(!block.hidden, "Test 3d, Plugin set to 'block' should have a visible 'block' action.");
    } else {
      ok(false, "Test 3d, Unexpected plugin '" + item.action.pluginName + "'");
    }
  }

  finishTest();
});
