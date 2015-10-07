var Ci = Components.interfaces;

const gCompleteState = Ci.nsIWebProgressListener.STATE_STOP +
                       Ci.nsIWebProgressListener.STATE_IS_NETWORK;

var gFrontProgressListener = {
  onProgressChange: function (aWebProgress, aRequest,
                              aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress) {
  },

  onStateChange: function (aWebProgress, aRequest, aStateFlags, aStatus) {
    var state = "onStateChange";
    info("FrontProgress: " + state + " 0x" + aStateFlags.toString(16));
    ok(gFrontNotificationsPos < gFrontNotifications.length, "Got an expected notification for the front notifications listener");
    is(state, gFrontNotifications[gFrontNotificationsPos], "Got a notification for the front notifications listener");
    gFrontNotificationsPos++;
  },

  onLocationChange: function (aWebProgress, aRequest, aLocationURI, aFlags) {
    var state = "onLocationChange";
    info("FrontProgress: " + state + " " + aLocationURI.spec);
    ok(gFrontNotificationsPos < gFrontNotifications.length, "Got an expected notification for the front notifications listener");
    is(state, gFrontNotifications[gFrontNotificationsPos], "Got a notification for the front notifications listener");
    gFrontNotificationsPos++;
  },
  
  onStatusChange: function (aWebProgress, aRequest, aStatus, aMessage) {
  },

  onSecurityChange: function (aWebProgress, aRequest, aState) {
    var state = "onSecurityChange";
    info("FrontProgress: " + state + " 0x" + aState.toString(16));
    ok(gFrontNotificationsPos < gFrontNotifications.length, "Got an expected notification for the front notifications listener");
    is(state, gFrontNotifications[gFrontNotificationsPos], "Got a notification for the front notifications listener");
    gFrontNotificationsPos++;
  }
}

var gAllProgressListener = {
  onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    var state = "onStateChange";
    info("AllProgress: " + state + " 0x" + aStateFlags.toString(16));
    ok(aBrowser == gTestBrowser, state + " notification came from the correct browser");
    ok(gAllNotificationsPos < gAllNotifications.length, "Got an expected notification for the all notifications listener");
    is(state, gAllNotifications[gAllNotificationsPos], "Got a notification for the all notifications listener");
    gAllNotificationsPos++;

    if ((aStateFlags & gCompleteState) == gCompleteState) {
      ok(gAllNotificationsPos == gAllNotifications.length, "Saw the expected number of notifications");
      ok(gFrontNotificationsPos == gFrontNotifications.length, "Saw the expected number of frontnotifications");
      executeSoon(gNextTest);
    }
  },

  onLocationChange: function (aBrowser, aWebProgress, aRequest, aLocationURI,
                              aFlags) {
    var state = "onLocationChange";
    info("AllProgress: " + state + " " + aLocationURI.spec);
    ok(aBrowser == gTestBrowser, state + " notification came from the correct browser");
    ok(gAllNotificationsPos < gAllNotifications.length, "Got an expected notification for the all notifications listener");
    is(state, gAllNotifications[gAllNotificationsPos], "Got a notification for the all notifications listener");
    gAllNotificationsPos++;
  },
  
  onStatusChange: function (aBrowser, aWebProgress, aRequest, aStatus, aMessage) {
    var state = "onStatusChange";
    ok(aBrowser == gTestBrowser, state + " notification came from the correct browser");
  },

  onSecurityChange: function (aBrowser, aWebProgress, aRequest, aState) {
    var state = "onSecurityChange";
    info("AllProgress: " + state + " 0x" + aState.toString(16));
    ok(aBrowser == gTestBrowser, state + " notification came from the correct browser");
    ok(gAllNotificationsPos < gAllNotifications.length, "Got an expected notification for the all notifications listener");
    is(state, gAllNotifications[gAllNotificationsPos], "Got a notification for the all notifications listener");
    gAllNotificationsPos++;
  }
}

var gFrontNotifications, gAllNotifications, gFrontNotificationsPos, gAllNotificationsPos;
var gBackgroundTab, gForegroundTab, gBackgroundBrowser, gForegroundBrowser, gTestBrowser;
var gTestPage = "/browser/browser/base/content/test/general/alltabslistener.html";
const kBasePage = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
var gNextTest;

function test() {
  waitForExplicitFinish();

  gBackgroundTab = gBrowser.addTab();
  gForegroundTab = gBrowser.addTab();
  gBackgroundBrowser = gBrowser.getBrowserForTab(gBackgroundTab);
  gForegroundBrowser = gBrowser.getBrowserForTab(gForegroundTab);
  gBrowser.selectedTab = gForegroundTab;

  // We must wait until a page has completed loading before
  // starting tests or we get notifications from that
  let promises = [
    waitForDocLoadComplete(gBackgroundBrowser),
    waitForDocLoadComplete(gForegroundBrowser)
  ];
  gBackgroundBrowser.loadURI(kBasePage);
  gForegroundBrowser.loadURI(kBasePage);
  Promise.all(promises).then(startTest1);
}

function runTest(browser, url, next) {
  gFrontNotificationsPos = 0;
  gAllNotificationsPos = 0;
  gNextTest = next;
  gTestBrowser = browser;
  browser.loadURI(url);
}

function startTest1() {
  info("\nTest 1");
  gBrowser.addProgressListener(gFrontProgressListener);
  gBrowser.addTabsProgressListener(gAllProgressListener);

  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    "onStateChange"
  ];
  gFrontNotifications = gAllNotifications;
  runTest(gForegroundBrowser, "http://example.org" + gTestPage, startTest2);
}

function startTest2() {
  info("\nTest 2");
  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    "onSecurityChange",
    "onStateChange"
  ];
  gFrontNotifications = gAllNotifications;
  runTest(gForegroundBrowser, "https://example.com" + gTestPage, startTest3);
}

function startTest3() {
  info("\nTest 3");
  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    "onStateChange"
  ];
  gFrontNotifications = [];
  runTest(gBackgroundBrowser, "http://example.org" + gTestPage, startTest4);
}

function startTest4() {
  info("\nTest 4");
  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    "onSecurityChange",
    "onStateChange"
  ];
  gFrontNotifications = [];
  runTest(gBackgroundBrowser, "https://example.com" + gTestPage, startTest5);
}

function startTest5() {
  info("\nTest 5");
  // Switch the foreground browser
  [gForegroundBrowser, gBackgroundBrowser] = [gBackgroundBrowser, gForegroundBrowser];
  [gForegroundTab, gBackgroundTab] = [gBackgroundTab, gForegroundTab];
  // Avoid the onLocationChange this will fire
  gBrowser.removeProgressListener(gFrontProgressListener);
  gBrowser.selectedTab = gForegroundTab;
  gBrowser.addProgressListener(gFrontProgressListener);

  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    "onStateChange"
  ];
  gFrontNotifications = gAllNotifications;
  runTest(gForegroundBrowser, "http://example.org" + gTestPage, startTest6);
}

function startTest6() {
  info("\nTest 6");
  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    "onStateChange"
  ];
  gFrontNotifications = [];
  runTest(gBackgroundBrowser, "http://example.org" + gTestPage, finishTest);
}

function finishTest() {
  gBrowser.removeProgressListener(gFrontProgressListener);
  gBrowser.removeTabsProgressListener(gAllProgressListener);
  gBrowser.removeTab(gBackgroundTab);
  gBrowser.removeTab(gForegroundTab);
  finish();
}
