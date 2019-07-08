/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests ensures the urlbar is cleared properly when about:home is visited.
 */

"use strict";

const { SessionSaver } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionSaver.jsm"
);
const { TabStateFlusher } = ChromeUtils.import(
  "resource:///modules/sessionstore/TabStateFlusher.jsm"
);

/**
 * Test what happens if loading a URL that should clear the
 * location bar after a parent process URL.
 */
add_task(async function clearURLBarAfterParentProcessURL() {
  let tab = await new Promise(resolve => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(
      gBrowser,
      "about:preferences"
    );
    let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
    newTabBrowser.addEventListener(
      "Initialized",
      async function() {
        resolve(gBrowser.selectedTab);
      },
      { capture: true, once: true }
    );
  });
  document.getElementById("home-button").click();
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    HomePage.get()
  );
  is(gURLBar.value, "", "URL bar should be empty");
  is(
    tab.linkedBrowser.userTypedValue,
    null,
    "The browser should have no recorded userTypedValue"
  );
  BrowserTestUtils.removeTab(tab);
});

/**
 * Same as above, but open the tab without passing the URL immediately
 * which changes behaviour in tabbrowser.xml.
 */
add_task(async function clearURLBarAfterParentProcessURLInExistingTab() {
  let tab = await new Promise(resolve => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
    let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
    newTabBrowser.addEventListener(
      "Initialized",
      async function() {
        resolve(gBrowser.selectedTab);
      },
      { capture: true, once: true }
    );
    BrowserTestUtils.loadURI(newTabBrowser, "about:preferences");
  });
  document.getElementById("home-button").click();
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    HomePage.get()
  );
  is(gURLBar.value, "", "URL bar should be empty");
  is(
    tab.linkedBrowser.userTypedValue,
    null,
    "The browser should have no recorded userTypedValue"
  );
  BrowserTestUtils.removeTab(tab);
});

/**
 * Load about:home directly from an about:newtab page. Because it is an
 * 'initial' page, we need to treat this specially if the user actually
 * loads a page like this from the URL bar.
 */
add_task(async function clearURLBarAfterManuallyLoadingAboutHome() {
  let promiseTabOpenedAndSwitchedTo = BrowserTestUtils.switchTab(
    gBrowser,
    () => {}
  );
  // This opens about:newtab:
  BrowserOpenTab();
  let tab = await promiseTabOpenedAndSwitchedTo;
  is(gURLBar.value, "", "URL bar should be empty");
  is(tab.linkedBrowser.userTypedValue, null, "userTypedValue should be null");

  gURLBar.value = "about:home";
  gURLBar.select();
  let aboutHomeLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "about:home"
  );
  EventUtils.sendKey("return");
  await aboutHomeLoaded;

  is(gURLBar.value, "", "URL bar should be empty");
  is(tab.linkedBrowser.userTypedValue, null, "userTypedValue should be null");
  BrowserTestUtils.removeTab(tab);
});

/**
 * Ensure we don't show 'about:home' in the URL bar temporarily in new tabs
 * while we're switching remoteness (when the URL we're loading and the
 * default content principal are different).
 */
add_task(async function dontTemporarilyShowAboutHome() {
  requestLongerTimeout(2);

  await SpecialPowers.pushPrefEnv({ set: [["browser.startup.page", 1]] });
  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  let win = OpenBrowserWindow();
  await windowOpenedPromise;
  let promiseTabSwitch = BrowserTestUtils.switchTab(win.gBrowser, () => {});
  win.BrowserOpenTab();
  await promiseTabSwitch;
  is(win.gBrowser.visibleTabs.length, 2, "2 tabs opened");
  await TabStateFlusher.flush(win.gBrowser.selectedBrowser);
  await BrowserTestUtils.closeWindow(win);
  ok(SessionStore.getClosedWindowCount(), "Should have a closed window");

  await SessionSaver.run();

  windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  win = SessionStore.undoCloseWindow(0);
  await windowOpenedPromise;
  let wpl = {
    onLocationChange() {
      is(win.gURLBar.value, "", "URL bar value should stay empty.");
    },
  };
  win.gBrowser.addProgressListener(wpl);

  if (win.gBrowser.visibleTabs.length < 2) {
    await BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  }
  let otherTab = win.gBrowser.selectedTab.previousElementSibling;
  let tabLoaded = BrowserTestUtils.browserLoaded(
    otherTab.linkedBrowser,
    false,
    "about:home"
  );
  await BrowserTestUtils.switchTab(win.gBrowser, otherTab);
  await tabLoaded;
  win.gBrowser.removeProgressListener(wpl);
  is(win.gURLBar.value, "", "URL bar value should be empty.");

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Test that if the Home Button is clicked after a user has typed
 * some value into the URL bar, that the URL bar is cleared if
 * the homepage is one of the initial pages set.
 */
add_task(async function() {
  await BrowserTestUtils.withNewTab(
    {
      url: "http://example.com",
      gBrowser,
    },
    async browser => {
      const TYPED_VALUE = "This string should get cleared";
      gURLBar.value = TYPED_VALUE;
      browser.userTypedValue = TYPED_VALUE;

      document.getElementById("home-button").click();
      await BrowserTestUtils.browserLoaded(browser, false, HomePage.get());
      is(gURLBar.value, "", "URL bar should be empty");
      is(
        browser.userTypedValue,
        null,
        "The browser should have no recorded userTypedValue"
      );
    }
  );
});
