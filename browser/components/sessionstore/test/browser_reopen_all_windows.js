"use strict";

const PATH = "browser/browser/components/sessionstore/test/empty.html";
var URLS_WIN1 = [
  "https://example.com/" + PATH,
  "https://example.org/" + PATH,
  "http://test1.mochi.test:8888/" + PATH,
  "http://test1.example.com/" + PATH,
];
var EXPECTED_URLS_WIN1 = ["about:blank", ...URLS_WIN1];

var URLS_WIN2 = [
  "http://sub1.test1.mochi.test:8888/" + PATH,
  "http://sub2.xn--lt-uia.mochi.test:8888/" + PATH,
  "http://test2.mochi.test:8888/" + PATH,
  "http://sub1.test2.example.org/" + PATH,
  "http://sub2.test1.example.org/" + PATH,
  "http://test2.example.com/" + PATH,
];
var EXPECTED_URLS_WIN2 = ["about:blank", ...URLS_WIN2];

add_task(async () => {
  requestLongerTimeout(4);

  await SpecialPowers.pushPrefEnv({
    set: [
      // Set the pref to true so we know exactly how many tabs should be restoring at
      // any given time. This guarantees that a finishing load won't start another.
      ["browser.sessionstore.restore_on_demand", false],
    ],
  });

  forgetClosedWindows();
  is(SessionStore.getClosedWindowCount(), 0, "starting with no closed windows");

  // Open window 1, with different tabs
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  for (let url of URLS_WIN1) {
    await BrowserTestUtils.openNewForegroundTab(win1.gBrowser, url);
  }

  // Open window 2, with different tabs
  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  for (let url of URLS_WIN2) {
    await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, url);
  }

  await TabStateFlusher.flushWindow(win1);
  await TabStateFlusher.flushWindow(win2);

  // Close both windows
  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await forceSaveState();

  // Verify both windows were accounted for by session store
  is(
    ss.getClosedWindowCount(),
    2,
    "The closed windows was added to Recently Closed Windows"
  );

  // Library -> History -> Recently Closed Windows -> Reopen all Windows

  // Show the Library view.
  info("About to open the PanelUI menu");
  let appMenu = document.getElementById("PanelUI-menu-button");
  let appMenuPopup = document.getElementById("appMenu-popup");
  let PopupShownPromise = BrowserTestUtils.waitForEvent(
    appMenuPopup,
    "popupshown"
  );
  appMenu.click();
  await PopupShownPromise;

  let promise;
  if (!gProton) {
    info("About to click on the Library option");

    document.getElementById("appMenu-library-button").click();
    let libraryView = document.getElementById("appMenu-libraryView");
    promise = BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
    await promise;
  }

  // Navigate to the History subview.
  info("About to click on the History option");
  let historyId = gProton
    ? "appMenu-history-button"
    : "appMenu-library-history-button";
  document.getElementById(historyId).click();
  let historyView = document.getElementById("PanelUI-history");
  promise = BrowserTestUtils.waitForEvent(historyView, "ViewShown");
  await promise;

  // Click on "Recently Closed Windows"
  info("About to click on 'Recently Closed Windows'");
  document.getElementById("appMenuRecentlyClosedWindows").click();
  let recentlyClosedWindowsView = document.getElementById(
    "appMenu-library-recentlyClosedWindows"
  );
  promise = BrowserTestUtils.waitForEvent(
    recentlyClosedWindowsView,
    "ViewShown"
  );
  await promise;

  // Click on "Restore All Windows"
  var restoreAll = document.getElementsByClassName(
    "restoreallitem subviewbutton subviewbutton-iconic panel-subview-footer-button"
  );

  // Wait for all windows to be restored
  var observedWin1 = TestUtils.topicObserved(
    "sessionstore-single-window-restored",
    subject => {
      return subject.gBrowser.tabs.length == EXPECTED_URLS_WIN1.length;
    }
  );
  var observedWin2 = TestUtils.topicObserved(
    "sessionstore-single-window-restored",
    subject => {
      return subject.gBrowser.tabs.length == EXPECTED_URLS_WIN2.length;
    }
  );
  var allPromises = Promise.all([observedWin1, observedWin2]);
  restoreAll[0].click();
  var restoredWindows = (await allPromises).map(([subject, data]) => subject);
  let allTabsRestored = PromiseUtils.defer();
  var tabsRestored = 0;
  function SSTabRestoredHandler() {
    tabsRestored++;
    info(`Received a SSTabRestored event`);
    if (tabsRestored == EXPECTED_URLS_WIN1.length + EXPECTED_URLS_WIN2.length) {
      info(`All tabs have been restored`);
      restoredWindows[0].gBrowser.tabContainer.removeEventListener(
        "SSTabRestored",
        SSTabRestoredHandler,
        true
      );
      restoredWindows[1].gBrowser.tabContainer.removeEventListener(
        "SSTabRestored",
        SSTabRestoredHandler,
        true
      );
      allTabsRestored.resolve();
    }
  }
  info("About to add an event listener for SSTabRestored");
  restoredWindows[0].gBrowser.tabContainer.addEventListener(
    "SSTabRestored",
    SSTabRestoredHandler,
    true
  );
  restoredWindows[1].gBrowser.tabContainer.addEventListener(
    "SSTabRestored",
    SSTabRestoredHandler,
    true
  );

  info("About to wait for tabs to be restored");
  await allTabsRestored.promise;

  is(
    restoredWindows[0].gBrowser.tabs.length,
    EXPECTED_URLS_WIN1.length,
    "All tabs restored"
  );
  is(
    restoredWindows[1].gBrowser.tabs.length,
    EXPECTED_URLS_WIN2.length,
    "All tabs restored"
  );

  // Verify that tabs opened as expected
  Assert.deepEqual(
    restoredWindows[0].gBrowser.tabs.map(
      tab => tab.linkedBrowser.currentURI.spec
    ),
    EXPECTED_URLS_WIN1
  );
  Assert.deepEqual(
    restoredWindows[1].gBrowser.tabs.map(
      tab => tab.linkedBrowser.currentURI.spec
    ),
    EXPECTED_URLS_WIN2
  );

  info("About to close windows");
  await BrowserTestUtils.closeWindow(restoredWindows[0]);
  await BrowserTestUtils.closeWindow(restoredWindows[1]);
});
