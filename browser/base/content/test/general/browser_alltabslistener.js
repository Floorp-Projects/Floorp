const gCompleteState =
  Ci.nsIWebProgressListener.STATE_STOP +
  Ci.nsIWebProgressListener.STATE_IS_NETWORK;

function getOriginalURL(request) {
  return request && request.QueryInterface(Ci.nsIChannel).originalURI.spec;
}

var gFrontProgressListener = {
  onProgressChange(
    aWebProgress,
    aRequest,
    aCurSelfProgress,
    aMaxSelfProgress,
    aCurTotalProgress,
    aMaxTotalProgress
  ) {},

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    var url = getOriginalURL(aRequest);
    if (url == "about:blank") {
      return;
    }
    var state = "onStateChange";
    info(
      "FrontProgress (" + url + "): " + state + " 0x" + aStateFlags.toString(16)
    );
    assertCorrectBrowserAndEventOrderForFront(state);
  },

  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    var url = getOriginalURL(aRequest);
    if (url == "about:blank") {
      return;
    }
    var state = "onLocationChange";
    info("FrontProgress: " + state + " " + aLocationURI.spec);
    assertCorrectBrowserAndEventOrderForFront(state);
  },

  onSecurityChange(aWebProgress, aRequest, aState) {
    var url = getOriginalURL(aRequest);
    if (url == "about:blank") {
      return;
    }
    var state = "onSecurityChange";
    info("FrontProgress (" + url + "): " + state + " 0x" + aState.toString(16));
    assertCorrectBrowserAndEventOrderForFront(state);
  },
};

function assertCorrectBrowserAndEventOrderForFront(aEventName) {
  Assert.less(
    gFrontNotificationsPos,
    gFrontNotifications.length,
    "Got an expected notification for the front notifications listener"
  );
  is(
    aEventName,
    gFrontNotifications[gFrontNotificationsPos],
    "Got a notification for the front notifications listener"
  );
  gFrontNotificationsPos++;
}

var gAllProgressListener = {
  onStateChange(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    var url = getOriginalURL(aRequest);
    if (url == "about:blank") {
      // ignore initial about blank
      return;
    }
    var state = "onStateChange";
    info(
      "AllProgress (" + url + "): " + state + " 0x" + aStateFlags.toString(16)
    );
    assertCorrectBrowserAndEventOrderForAll(state, aBrowser);
    assertReceivedFlags(
      state,
      gAllNotifications[gAllNotificationsPos],
      aStateFlags
    );
    gAllNotificationsPos++;

    if ((aStateFlags & gCompleteState) == gCompleteState) {
      is(
        gAllNotificationsPos,
        gAllNotifications.length,
        "Saw the expected number of notifications"
      );
      is(
        gFrontNotificationsPos,
        gFrontNotifications.length,
        "Saw the expected number of frontnotifications"
      );
      executeSoon(gNextTest);
    }
  },

  onLocationChange(aBrowser, aWebProgress, aRequest, aLocationURI, aFlags) {
    var url = getOriginalURL(aRequest);
    if (url == "about:blank") {
      // ignore initial about blank
      return;
    }
    var state = "onLocationChange";
    info("AllProgress: " + state + " " + aLocationURI.spec);
    assertCorrectBrowserAndEventOrderForAll(state, aBrowser);
    assertReceivedFlags(
      "onLocationChange",
      gAllNotifications[gAllNotificationsPos],
      aFlags
    );
    gAllNotificationsPos++;
  },

  onSecurityChange(aBrowser, aWebProgress, aRequest, aState) {
    var url = getOriginalURL(aRequest);
    if (url == "about:blank") {
      // ignore initial about blank
      return;
    }
    var state = "onSecurityChange";
    info("AllProgress (" + url + "): " + state + " 0x" + aState.toString(16));
    assertCorrectBrowserAndEventOrderForAll(state, aBrowser);
    is(
      state,
      gAllNotifications[gAllNotificationsPos],
      "Got a notification for the all notifications listener"
    );
    gAllNotificationsPos++;
  },
};

function assertCorrectBrowserAndEventOrderForAll(aState, aBrowser) {
  Assert.equal(
    aBrowser,
    gTestBrowser,
    aState + " notification came from the correct browser"
  );
  Assert.less(
    gAllNotificationsPos,
    gAllNotifications.length,
    "Got an expected notification for the all notifications listener"
  );
}

function assertReceivedFlags(aState, aObjOrEvent, aFlags) {
  if (aObjOrEvent !== null && typeof aObjOrEvent === "object") {
    is(
      aState,
      aObjOrEvent.state,
      "Got a notification for the all notifications listener"
    );
    is(aFlags, aFlags & aObjOrEvent.flags, `Got correct flags for ${aState}`);
  } else {
    is(
      aState,
      aObjOrEvent,
      "Got a notification for the all notifications listener"
    );
  }
}

var gFrontNotifications,
  gAllNotifications,
  gFrontNotificationsPos,
  gAllNotificationsPos;
var gBackgroundTab,
  gForegroundTab,
  gBackgroundBrowser,
  gForegroundBrowser,
  gTestBrowser;
var gTestPage =
  "/browser/browser/base/content/test/general/alltabslistener.html";
const kBasePage =
  "http://mochi.test:8888/browser/browser/base/content/test/general/dummy_page.html";
var gNextTest;

async function test() {
  waitForExplicitFinish();

  gBackgroundTab = BrowserTestUtils.addTab(gBrowser);
  gForegroundTab = BrowserTestUtils.addTab(gBrowser);
  gBackgroundBrowser = gBrowser.getBrowserForTab(gBackgroundTab);
  gForegroundBrowser = gBrowser.getBrowserForTab(gForegroundTab);
  gBrowser.selectedTab = gForegroundTab;

  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    "onStateChange",
  ];

  // We must wait until a page has completed loading before
  // starting tests or we get notifications from that
  let promises = [
    BrowserTestUtils.browserStopped(gBackgroundBrowser, kBasePage),
    BrowserTestUtils.browserStopped(gForegroundBrowser, kBasePage),
  ];
  BrowserTestUtils.startLoadingURIString(gBackgroundBrowser, kBasePage);
  BrowserTestUtils.startLoadingURIString(gForegroundBrowser, kBasePage);
  await Promise.all(promises);
  // If we process switched, the tabbrowser may still be processing the state_stop
  // notification here because of how microtasks work. Ensure that that has
  // happened before starting to test (which would add listeners to the tabbrowser
  // which would get confused by being called about kBasePage loading).
  await new Promise(executeSoon);
  startTest1();
}

function runTest(browser, url, next) {
  gFrontNotificationsPos = 0;
  gAllNotificationsPos = 0;
  gNextTest = next;
  gTestBrowser = browser;
  BrowserTestUtils.startLoadingURIString(browser, url);
}

function startTest1() {
  info("\nTest 1");
  gBrowser.addProgressListener(gFrontProgressListener);
  gBrowser.addTabsProgressListener(gAllProgressListener);

  gFrontNotifications = gAllNotifications;
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  runTest(gForegroundBrowser, "http://example.org" + gTestPage, startTest2);
}

function startTest2() {
  info("\nTest 2");
  gFrontNotifications = gAllNotifications;
  runTest(gForegroundBrowser, "https://example.com" + gTestPage, startTest3);
}

function startTest3() {
  info("\nTest 3");
  gFrontNotifications = [];
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  runTest(gBackgroundBrowser, "http://example.org" + gTestPage, startTest4);
}

function startTest4() {
  info("\nTest 4");
  gFrontNotifications = [];
  runTest(gBackgroundBrowser, "https://example.com" + gTestPage, startTest5);
}

function startTest5() {
  info("\nTest 5");
  // Switch the foreground browser
  [gForegroundBrowser, gBackgroundBrowser] = [
    gBackgroundBrowser,
    gForegroundBrowser,
  ];
  [gForegroundTab, gBackgroundTab] = [gBackgroundTab, gForegroundTab];
  // Avoid the onLocationChange this will fire
  gBrowser.removeProgressListener(gFrontProgressListener);
  gBrowser.selectedTab = gForegroundTab;
  gBrowser.addProgressListener(gFrontProgressListener);

  gFrontNotifications = gAllNotifications;
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  runTest(gForegroundBrowser, "http://example.org" + gTestPage, startTest6);
}

function startTest6() {
  info("\nTest 6");
  gFrontNotifications = [];
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  runTest(gBackgroundBrowser, "http://example.org" + gTestPage, startTest7);
}

// Navigate from remote to non-remote
function startTest7() {
  info("\nTest 7");
  gFrontNotifications = [];
  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    {
      state: "onLocationChange",
      flags: Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT,
    }, // dummy onLocationChange event
    "onStateChange",
  ];
  runTest(gBackgroundBrowser, "about:preferences", startTest8);
}

// Navigate from non-remote to non-remote
function startTest8() {
  info("\nTest 8");
  gFrontNotifications = [];
  gAllNotifications = [
    "onStateChange",
    {
      state: "onStateChange",
      flags:
        Ci.nsIWebProgressListener.STATE_IS_REDIRECTED_DOCUMENT |
        Ci.nsIWebProgressListener.STATE_IS_REQUEST |
        Ci.nsIWebProgressListener.STATE_START,
    },
    "onLocationChange",
    "onSecurityChange",
    "onStateChange",
  ];
  runTest(gBackgroundBrowser, "about:config", startTest9);
}

// Navigate from non-remote to remote
function startTest9() {
  info("\nTest 9");
  gFrontNotifications = [];
  gAllNotifications = [
    "onStateChange",
    "onLocationChange",
    "onSecurityChange",
    "onStateChange",
  ];
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  runTest(gBackgroundBrowser, "http://example.org" + gTestPage, finishTest);
}

function finishTest() {
  gBrowser.removeProgressListener(gFrontProgressListener);
  gBrowser.removeTabsProgressListener(gAllProgressListener);
  gBrowser.removeTab(gBackgroundTab);
  gBrowser.removeTab(gForegroundTab);
  finish();
}
