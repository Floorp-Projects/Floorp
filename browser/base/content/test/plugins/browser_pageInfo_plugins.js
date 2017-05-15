var gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPageInfo = null;
var gNextTest = null;
var gTestBrowser = null;
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"]
                    .getService(Components.interfaces.nsIPluginHost);
var gPermissionManager = Components.classes["@mozilla.org/permissionmanager;1"]
                           .getService(Components.interfaces.nsIPermissionManager);
var gTestPermissionString = gPluginHost.getPermissionStringForType("application/x-test");
var gSecondTestPermissionString = gPluginHost.getPermissionStringForType("application/x-second-test");

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
  Services.obs.addObserver(pageInfoObserve, "page-info-dialog-loaded");
  gNextTest = continuation;
  // An explanation: it looks like the test harness complains about leaked
  // windows if we don't keep a reference to every window we've opened.
  // So, don't reuse pointers to opened Page Info windows - simply append
  // to this list.
  gPageInfo = BrowserPageInfo(null, "permTab");
}

function pageInfoObserve(win, topic, data) {
  Services.obs.removeObserver(pageInfoObserve, "page-info-dialog-loaded");
  gPageInfo.onFinished.push(() => executeSoon(gNextTest));
}

function finishTest() {
  gPermissionManager.remove(makeURI("http://127.0.0.1:8888/"), gTestPermissionString);
  gPermissionManager.remove(makeURI("http://127.0.0.1:8888/"), gSecondTestPermissionString);
  Services.prefs.clearUserPref("plugins.click_to_play");
  gBrowser.removeCurrentTab();

  gPageInfo = null;
  gNextTest = null;
  gTestBrowser = null;
  gPluginHost = null;
  gPermissionManager = null;

  executeSoon(finish);
}

function test() {
  waitForExplicitFinish();
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;
  gPermissionManager.remove(makeURI("http://127.0.0.1:8888/"), gTestPermissionString);
  gPermissionManager.remove(makeURI("http://127.0.0.1:8888/"), gSecondTestPermissionString);
  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart1a);
}

// The first test plugin is CtP and the second test plugin is enabled.
function testPart1a() {
  let testElement = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = testElement.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "part 1a: Test plugin should not be activated");
  let secondtest = gTestBrowser.contentDocument.getElementById("secondtestA");
  objLoadingContent = secondtest.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "part 1a: Second Test plugin should be activated");

  doOnOpenPageInfo(testPart1b);
}

function testPart1b() {
  let testRadioGroup = gPageInfo.document.getElementById(gTestPermissionString + "RadioGroup");
  let testRadioDefault = gPageInfo.document.getElementById(gTestPermissionString + "#0");

  is(testRadioGroup.selectedItem, testRadioDefault, "part 1b: Test radio group should be set to 'Default'");
  let testRadioAllow = gPageInfo.document.getElementById(gTestPermissionString + "#1");
  testRadioGroup.selectedItem = testRadioAllow;
  testRadioAllow.doCommand();

  let secondtestRadioGroup = gPageInfo.document.getElementById(gSecondTestPermissionString + "RadioGroup");
  let secondtestRadioDefault = gPageInfo.document.getElementById(gSecondTestPermissionString + "#0");
  is(secondtestRadioGroup.selectedItem, secondtestRadioDefault, "part 1b: Second Test radio group should be set to 'Default'");
  let secondtestRadioAsk = gPageInfo.document.getElementById(gSecondTestPermissionString + "#3");
  secondtestRadioGroup.selectedItem = secondtestRadioAsk;
  secondtestRadioAsk.doCommand();

  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart2);
}

// Now, the Test plugin should be allowed, and the Test2 plugin should be CtP
function testPart2() {
  let testElement = gTestBrowser.contentDocument.getElementById("test").
    QueryInterface(Ci.nsIObjectLoadingContent);
  ok(testElement.activated, "part 2: Test plugin should be activated");

  let secondtest = gTestBrowser.contentDocument.getElementById("secondtestA").
    QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!secondtest.activated, "part 2: Second Test plugin should not be activated");
  is(secondtest.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "part 2: Second test plugin should be click-to-play.");

  let testRadioGroup = gPageInfo.document.getElementById(gTestPermissionString + "RadioGroup");
  let testRadioAllow = gPageInfo.document.getElementById(gTestPermissionString + "#1");
  is(testRadioGroup.selectedItem, testRadioAllow, "part 2: Test radio group should be set to 'Allow'");
  let testRadioBlock = gPageInfo.document.getElementById(gTestPermissionString + "#2");
  testRadioGroup.selectedItem = testRadioBlock;
  testRadioBlock.doCommand();

  let secondtestRadioGroup = gPageInfo.document.getElementById(gSecondTestPermissionString + "RadioGroup");
  let secondtestRadioAsk = gPageInfo.document.getElementById(gSecondTestPermissionString + "#3");
  is(secondtestRadioGroup.selectedItem, secondtestRadioAsk, "part 2: Second Test radio group should be set to 'Always Ask'");
  let secondtestRadioBlock = gPageInfo.document.getElementById(gSecondTestPermissionString + "#2");
  secondtestRadioGroup.selectedItem = secondtestRadioBlock;
  secondtestRadioBlock.doCommand();

  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart3);
}

// Now, all the things should be blocked
function testPart3() {
  let testElement = gTestBrowser.contentDocument.getElementById("test").
    QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!testElement.activated, "part 3: Test plugin should not be activated");
  is(testElement.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_DISABLED,
    "part 3: Test plugin should be marked as PLUGIN_DISABLED");

  let secondtest = gTestBrowser.contentDocument.getElementById("secondtestA").
    QueryInterface(Ci.nsIObjectLoadingContent);

  ok(!secondtest.activated, "part 3: Second Test plugin should not be activated");
  is(secondtest.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_DISABLED,
     "part 3: Second test plugin should be marked as PLUGIN_DISABLED");

  // reset permissions
  gPermissionManager.remove(makeURI("http://127.0.0.1:8888/"), gTestPermissionString);
  gPermissionManager.remove(makeURI("http://127.0.0.1:8888/"), gSecondTestPermissionString);
  // check that changing the permissions affects the radio state in the
  // open Page Info window
  let testRadioGroup = gPageInfo.document.getElementById(gTestPermissionString + "RadioGroup");
  let testRadioDefault = gPageInfo.document.getElementById(gTestPermissionString + "#0");
  is(testRadioGroup.selectedItem, testRadioDefault, "part 3: Test radio group should be set to 'Default'");
  let secondtestRadioGroup = gPageInfo.document.getElementById(gSecondTestPermissionString + "RadioGroup");
  let secondtestRadioDefault = gPageInfo.document.getElementById(gSecondTestPermissionString + "#0");
  is(secondtestRadioGroup.selectedItem, secondtestRadioDefault, "part 3: Second Test radio group should be set to 'Default'");

  doOnPageLoad(gHttpTestRoot + "plugin_two_types.html", testPart4a);
}

// Now test that setting permission directly (as from the popup notification)
// immediately influences Page Info.
function testPart4a() {
  // simulate "allow" from the doorhanger
  gPermissionManager.add(gTestBrowser.currentURI, gTestPermissionString, Ci.nsIPermissionManager.ALLOW_ACTION);
  gPermissionManager.add(gTestBrowser.currentURI, gSecondTestPermissionString, Ci.nsIPermissionManager.ALLOW_ACTION);

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
  finishTest();
}
