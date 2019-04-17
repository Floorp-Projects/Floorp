var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

var gTestBrowser = null;
var gNextTest = null;
var gPluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

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
  gPrivateBrowser = gPrivateWindow.gBrowser.selectedBrowser;

  BrowserTestUtils.loadURI(gPrivateBrowser, url);
  await BrowserTestUtils.browserLoaded(gPrivateBrowser, false, url);
  info("loaded " + url);
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
  is(gPrivateWindow.PopupNotifications.panel.firstElementChild.checkbox.hidden, true, "'Remember' checkbox should be hidden in private windows");

  let promises = [
    BrowserTestUtils.browserLoaded(gTestBrowser, false, gHttpTestRoot + "plugin_test.html"),
    BrowserTestUtils.waitForEvent(window, "activate"),
  ];
  gPrivateWindow.close();
  BrowserTestUtils.loadURI(gTestBrowser, gHttpTestRoot + "plugin_test.html");
  await Promise.all(promises);
  await SimpleTest.promiseFocus(window);
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

  PopupNotifications.panel.firstElementChild.button.click();

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

  let popupNotification = await TestUtils.waitForCondition(() => {
    return gPrivateWindow.PopupNotifications.getNotification("click-to-play-plugins", gPrivateBrowser);
  }, "Waiting for click-to-play-plugins notification in the private window");
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
  is(gPrivateWindow.PopupNotifications.panel.firstElementChild.secondaryButton.hidden, true,
     "Test 2c, Activated plugin in a private window should not have visible 'Block' button.");
  is(gPrivateWindow.PopupNotifications.panel.firstElementChild.checkbox.hidden, true,
     "Test 2c, Activated plugin in a private window should not have visible 'Remember' checkbox.");

  clearAllPluginPermissions();

  let promises = [
    BrowserTestUtils.browserLoaded(gTestBrowser, false, gHttpTestRoot + "plugin_test.html"),
    BrowserTestUtils.waitForEvent(window, "activate"),
  ];
  gPrivateWindow.close();
  BrowserTestUtils.loadURI(gTestBrowser, gHttpTestRoot + "plugin_test.html");
  await Promise.all(promises);
  await SimpleTest.promiseFocus(window);
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

  // Simulate clicking the "Allow" button.
  let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                   "Shown");
  popupNotification.reshow();
  await promiseShown;
  PopupNotifications.panel.firstElementChild.button.click();

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

  let popupNotification = await TestUtils.waitForCondition(() => {
    return gPrivateWindow.PopupNotifications.getNotification("click-to-play-plugins", gPrivateBrowser);
  }, "Waiting for click-to-play-plugins notification in the private window");
  ok(popupNotification, "Test 3c, Should have a click-to-play notification");

  // Check the button status
  let promiseShown = BrowserTestUtils.waitForEvent(gPrivateWindow.PopupNotifications.panel,
                                                   "Shown");
  popupNotification.reshow();
  await promiseShown;
  is(gPrivateWindow.PopupNotifications.panel.firstElementChild.secondaryButton.hidden, true,
     "Test 2c, Activated plugin in a private window should not have visible 'Block' button.");
  is(gPrivateWindow.PopupNotifications.panel.firstElementChild.checkbox.hidden, true,
     "Test 2c, Activated plugin in a private window should not have visible 'Remember' checkbox.");

  BrowserTestUtils.loadURI(gPrivateBrowser, gHttpTestRoot + "plugin_two_types.html");
  await BrowserTestUtils.browserLoaded(gPrivateBrowser);

  finishTest();
});
