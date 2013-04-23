let gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
let gPageInfo = null;
let gNextTest = null;
let gTestBrowser = null;
let gPluginHost = Components.classes["@mozilla.org/plugin/host;1"]
                    .getService(Components.interfaces.nsIPluginHost);
let gPermissionManager = Components.classes["@mozilla.org/permissionmanager;1"]
                           .getService(Components.interfaces.nsIPermissionManager);
let gTestPermissionString = gPluginHost.getPermissionStringForType("application/x-test");
let gSecondTestPermissionString = gPluginHost.getPermissionStringForType("application/x-second-test");

function doOnPageLoad(url, continuation) {
  gNextTest = continuation;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.contentWindow.location = url;
}

function pageLoad() {
  gTestBrowser.removeEventListener("load", pageLoad);
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we then go on to test the states
  executeSoon(gNextTest);
}

function doOnOpenPageInfo(continuation) {
  Services.obs.addObserver(pageInfoObserve, "page-info-dialog-loaded", false);
  gNextTest = continuation;
  // An explanation: it looks like the test harness complains about leaked
  // windows if we don't keep a reference to every window we've opened.
  // So, don't reuse pointers to opened Page Info windows - simply append
  // to this list.
  gPageInfo = BrowserPageInfo(null, "permTab");
}

function pageInfoObserve(win, topic, data) {
  Services.obs.removeObserver(pageInfoObserve, "page-info-dialog-loaded");
  executeSoon(gNextTest);
}

function finishTest() {
  gPermissionManager.remove("127.0.0.1:8888", gTestPermissionString);
  gPermissionManager.remove("127.0.0.1:8888", gSecondTestPermissionString);
  Services.prefs.clearUserPref("plugins.click_to_play");
  getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  gBrowser.removeCurrentTab();
  finish();
}

function test() {
  waitForExplicitFinish();
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  getTestPlugin("Second Test Plug-in").enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart1a);
}

// By default, everything should be click-to-play. So: no plugins should be
// activated, and the radio buttons in Page Info should be "Always Ask"
function testPart1a() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "part 1a: should have a click-to-play notification");
  let test = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = test.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "part 1a: Test plugin should not be activated");
  let secondtest = gTestBrowser.contentDocument.getElementById("secondtestA");
  let objLoadingContent = secondtest.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "part 1a: Second Test plugin should not be activated");

  doOnOpenPageInfo(testPart1b);
}

function testPart1b() {
  let testRadioGroup = gPageInfo.document.getElementById(gTestPermissionString + "RadioGroup");
  let testRadioAsk = gPageInfo.document.getElementById(gTestPermissionString + "#0");
  is(testRadioGroup.selectedItem, testRadioAsk, "part 1b: Test radio group should be set to 'Always Ask'");
  let testRadioAllow = gPageInfo.document.getElementById(gTestPermissionString + "#1");
  testRadioGroup.selectedItem = testRadioAllow;
  testRadioAllow.doCommand();

  let secondtestRadioGroup = gPageInfo.document.getElementById(gSecondTestPermissionString + "RadioGroup");
  let secondtestRadioAsk = gPageInfo.document.getElementById(gSecondTestPermissionString + "#0");
  is(secondtestRadioGroup.selectedItem, secondtestRadioAsk, "part 1b: Second Test radio group should be set to 'Always Ask'");

  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart2);
}

// Now, the Test plugin should be allowed.
function testPart2() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "part 2: should have a click-to-play notification");
  let test = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = test.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "part 2: Test plugin should be activated");
  let secondtest = gTestBrowser.contentDocument.getElementById("secondtestA");
  let objLoadingContent = secondtest.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "part 2: Second Test plugin should not be activated");

  let testRadioGroup = gPageInfo.document.getElementById(gTestPermissionString + "RadioGroup");
  let testRadioAllow = gPageInfo.document.getElementById(gTestPermissionString + "#1");
  is(testRadioGroup.selectedItem, testRadioAllow, "part 2: Test radio group should be set to 'Allow'");
  let testRadioBlock = gPageInfo.document.getElementById(gTestPermissionString + "#2");
  testRadioGroup.selectedItem = testRadioBlock;
  testRadioBlock.doCommand();

  let secondtestRadioGroup = gPageInfo.document.getElementById(gSecondTestPermissionString + "RadioGroup");
  let secondtestRadioAsk = gPageInfo.document.getElementById(gSecondTestPermissionString + "#0");
  is(secondtestRadioGroup.selectedItem, secondtestRadioAsk, "part 2: Second Test radio group should be set to 'Always Ask'");
  let secondtestRadioBlock = gPageInfo.document.getElementById(gSecondTestPermissionString + "#2");
  secondtestRadioGroup.selectedItem = secondtestRadioBlock;
  secondtestRadioBlock.doCommand();

  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart3);
}

// Now, all the things should be blocked
function testPart3() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!notification, "part 3: should not have a click-to-play notification");

  let test = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = test.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "part 3: Test plugin should not be activated");
  let overlay = gTestBrowser.contentDocument.getAnonymousElementByAttribute(test, "class", "mainBox");
  ok(overlay.style.visibility == "hidden", "part 3: Test plugin should not have visible overlay");
  let secondtest = gTestBrowser.contentDocument.getElementById("secondtestA");
  let objLoadingContent = secondtest.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "part 3: Second Test plugin should not be activated");
  let overlay = gTestBrowser.contentDocument.getAnonymousElementByAttribute(secondtest, "class", "mainBox");
  ok(overlay.style.visibility == "hidden", "part 3: Second Test plugin should not have visible overlay");

  // reset permissions
  gPermissionManager.remove("127.0.0.1:8888", gTestPermissionString);
  gPermissionManager.remove("127.0.0.1:8888", gSecondTestPermissionString);
  // check that changing the permissions affects the radio state in the
  // open Page Info window
  let testRadioGroup = gPageInfo.document.getElementById(gTestPermissionString + "RadioGroup");
  let testRadioAsk = gPageInfo.document.getElementById(gTestPermissionString + "#0");
  is(testRadioGroup.selectedItem, testRadioAsk, "part 3: Test radio group should be set to 'Ask'");
  let secondtestRadioGroup = gPageInfo.document.getElementById(gSecondTestPermissionString + "RadioGroup");
  let secondtestRadioAsk = gPageInfo.document.getElementById(gSecondTestPermissionString + "#0");
  is(secondtestRadioGroup.selectedItem, secondtestRadioAsk, "part 3: Second Test radio group should be set to 'Always Ask'");

  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart4a);
}

// Now test that the popup notification influences Page Info
function testPart4a() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "part 4a: should have a notification");
  // simulate "always allow"
  notification.secondaryActions[0].callback();

  // check (again) that changing the permissions affects the radio state in the
  // open Page Info window
  let testRadioGroup = gPageInfo.document.getElementById(gTestPermissionString + "RadioGroup");
  let testRadioAllow = gPageInfo.document.getElementById(gTestPermissionString + "#1");
  is(testRadioGroup.selectedItem, testRadioAllow, "part 4a: Test radio group should be set to 'Allow'");
  let secondtestRadioGroup = gPageInfo.document.getElementById(gSecondTestPermissionString + "RadioGroup");
  let secondtestRadioAllow = gPageInfo.document.getElementById(gSecondTestPermissionString + "#1");
  is(secondtestRadioGroup.selectedItem, secondtestRadioAllow, "part 4a: Second Test radio group should be set to 'Always Allow'");

  // now close Page Info and see that it opens with the right settings
  gPageInfo.close();
  doOnOpenPageInfo(testPart4b);
}

// check that "always allow" resulted in the radio buttons getting set to allow
function testPart4b() {
  let testRadioGroup = gPageInfo.document.getElementById(gTestPermissionString + "RadioGroup");
  let testRadioAllow = gPageInfo.document.getElementById(gTestPermissionString + "#1");
  is(testRadioGroup.selectedItem, testRadioAllow, "part 4b: Test radio group should be set to 'Allow'");

  let secondtestRadioGroup = gPageInfo.document.getElementById(gSecondTestPermissionString + "RadioGroup");
  let secondtestRadioAllow = gPageInfo.document.getElementById(gSecondTestPermissionString + "#1");
  is(secondtestRadioGroup.selectedItem, secondtestRadioAllow, "part 4b: Second Test radio group should be set to 'Allow'");

  Services.prefs.setBoolPref("plugins.click_to_play", false);
  gPageInfo.close();
  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart5a);
}

// check that if there are no click-to-play plugins, the plugin row is hidden
function testPart5a() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!notification, "part 5a: should not have a click-to-play notification");

  doOnOpenPageInfo(testPart5b);
}

function testPart5b() {
  ok(gPageInfo.document.getElementById("permPluginsRow").hidden, "part 5b: plugin permission row should be hidden");

  gPageInfo.close();
  setAndUpdateBlocklist(gHttpTestRoot + "blockPluginVulnerableUpdatable.xml",
  function() {
    doOnPageLoad(gHttpTestRoot + "plugin_test.html", testPart6a);
  });
}

// check that if plugins.click_to_play is false, but there is a
// click-to-play blocklisted plugin, we show the plugin row
function testPart6a() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "part 6a: should have a click-to-play notification");

  doOnOpenPageInfo(testPart6b);
}

function testPart6b() {
  ok(!gPageInfo.document.getElementById("permPluginsRow").hidden, "part 6b: plugin permission row should not be hidden");

  setAndUpdateBlocklist(gHttpTestRoot + "blockNoPlugins.xml",
  function() {
    resetBlocklist();
    gPageInfo.close();
    finishTest();
  });
}
