"use strict";

const {SessionSaver} = Cu.import("resource:///modules/sessionstore/SessionSaver.jsm", {});
const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

/**
 * Test what happens if loading a URL that should clear the
 * location bar after a parent process URL.
 */
add_task(function* clearURLBarAfterParentProcessURL() {
  let tab = yield new Promise(resolve => {
    gBrowser.selectedTab = gBrowser.addTab("about:preferences");
    let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
    newTabBrowser.addEventListener("Initialized", function onInit() {
      newTabBrowser.removeEventListener("Initialized", onInit, true);
      resolve(gBrowser.selectedTab);
    }, true);
  });
  document.getElementById("home-button").click();
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  is(gURLBar.value, "", "URL bar should be empty");
  is(tab.linkedBrowser.userTypedValue, null, "The browser should have no recorded userTypedValue");
  yield BrowserTestUtils.removeTab(tab);
});

/**
 * Same as above, but open the tab without passing the URL immediately
 * which changes behaviour in tabbrowser.xml.
 */
add_task(function* clearURLBarAfterParentProcessURLInExistingTab() {
  let tab = yield new Promise(resolve => {
    gBrowser.selectedTab = gBrowser.addTab();
    let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
    newTabBrowser.addEventListener("Initialized", function onInit() {
      newTabBrowser.removeEventListener("Initialized", onInit, true);
      resolve(gBrowser.selectedTab);
    }, true);
    newTabBrowser.loadURI("about:preferences");
  });
  document.getElementById("home-button").click();
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  is(gURLBar.value, "", "URL bar should be empty");
  is(tab.linkedBrowser.userTypedValue, null, "The browser should have no recorded userTypedValue");
  yield BrowserTestUtils.removeTab(tab);
});

/**
 * Load about:home directly from an about:newtab page. Because it is an
 * 'initial' page, we need to treat this specially if the user actually
 * loads a page like this from the URL bar.
 */
add_task(function* clearURLBarAfterManuallyLoadingAboutHome() {
  let promiseTabOpenedAndSwitchedTo = BrowserTestUtils.switchTab(gBrowser, () => {});
  // This opens about:newtab:
  BrowserOpenTab();
  let tab = yield promiseTabOpenedAndSwitchedTo;
  is(gURLBar.value, "", "URL bar should be empty");
  is(tab.linkedBrowser.userTypedValue, null, "userTypedValue should be null");

  gURLBar.value = "about:home";
  gURLBar.select();
  let aboutHomeLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, "about:home");
  EventUtils.sendKey("return");
  yield aboutHomeLoaded;

  is(gURLBar.value, "", "URL bar should be empty");
  is(tab.linkedBrowser.userTypedValue, null, "userTypedValue should be null");
  yield BrowserTestUtils.removeTab(tab);
});

/**
 * Ensure we don't show 'about:home' in the URL bar temporarily in new tabs
 * while we're switching remoteness (when the URL we're loading and the
 * default content principal are different).
 */
add_task(function* dontTemporarilyShowAboutHome() {
  yield SpecialPowers.pushPrefEnv({set: [["browser.startup.page", 1]]});
  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  let win = OpenBrowserWindow();
  yield windowOpenedPromise;
  let promiseTabSwitch = BrowserTestUtils.switchTab(win.gBrowser, () => {});
  win.BrowserOpenTab();
  yield promiseTabSwitch;
  yield TabStateFlusher.flush(win.gBrowser.selectedBrowser);
  yield BrowserTestUtils.closeWindow(win);
  ok(SessionStore.getClosedWindowCount(), "Should have a closed window");

  yield SessionSaver.run();

  windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  win = SessionStore.undoCloseWindow(0);
  yield windowOpenedPromise;
  let wpl = {
    onLocationChange() {
      is(win.gURLBar.value, "", "URL bar value should stay empty.");
    },
  };
  win.gBrowser.addProgressListener(wpl);
  let otherTab = win.gBrowser.selectedTab.previousSibling;
  let tabLoaded = BrowserTestUtils.browserLoaded(otherTab.linkedBrowser, false, "about:home");
  yield BrowserTestUtils.switchTab(win.gBrowser, otherTab);
  yield tabLoaded;
  win.gBrowser.removeProgressListener(wpl);
  is(win.gURLBar.value, "", "URL bar value should be empty.");

  yield BrowserTestUtils.closeWindow(win);
});
