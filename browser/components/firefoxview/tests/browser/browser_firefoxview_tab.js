/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function() {
  if (!Services.prefs.getBoolPref("browser.tabs.firefox-view")) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.tabs.firefox-view", true]],
    });
    CustomizableUI.addWidgetToArea(
      "firefox-view-button",
      CustomizableUI.AREA_TABSTRIP,
      0
    );
    registerCleanupFunction(() => {
      CustomizableUI.removeWidgetFromArea("firefox-view-button");
    });
  }
});

function assertFirefoxViewTab(w = window) {
  ok(w.FirefoxViewHandler.tab, "Firefox View tab exists");
  ok(w.FirefoxViewHandler.tab?.hidden, "Firefox View tab is hidden");
  is(
    w.gBrowser.tabs.indexOf(w.FirefoxViewHandler.tab),
    0,
    "Firefox View tab is the first tab"
  );
  is(
    w.gBrowser.visibleTabs.indexOf(w.FirefoxViewHandler.tab),
    -1,
    "Firefox View tab is not in the list of visible tabs"
  );
}

async function openFirefoxViewTab(w = window) {
  ok(
    !w.FirefoxViewHandler.tab,
    "Firefox View tab doesn't exist prior to clicking the button"
  );
  info("Clicking the Firefox View button");
  await EventUtils.synthesizeMouseAtCenter(
    w.document.getElementById("firefox-view-button"),
    {},
    w
  );
  assertFirefoxViewTab(w);
  is(w.gBrowser.tabContainer.selectedIndex, 0, "Firefox View tab is selected");
  await BrowserTestUtils.browserLoaded(w.FirefoxViewHandler.tab.linkedBrowser);
  return w.FirefoxViewHandler.tab;
}

function closeFirefoxViewTab(w = window) {
  w.gBrowser.removeTab(w.FirefoxViewHandler.tab);
  ok(
    !w.FirefoxViewHandler.tab,
    "Reference to Firefox View tab got removed when closing the tab"
  );
}

add_task(async function load_opens_new_tab() {
  await openFirefoxViewTab();
  gURLBar.focus();
  gURLBar.value = "https://example.com";
  let newTabOpened = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  EventUtils.synthesizeKey("KEY_Enter");
  info(
    "Waiting for new tab to open from the address bar in the Firefox View tab"
  );
  await newTabOpened;
  assertFirefoxViewTab();
  isnot(
    gBrowser.tabContainer.selectedIndex,
    0,
    "Firefox View tab is not selected anymore (new tab opened in the foreground)"
  );
  gBrowser.removeCurrentTab();
  closeFirefoxViewTab();
});

add_task(async function number_tab_select_shortcut() {
  await openFirefoxViewTab();
  EventUtils.synthesizeKey(
    "1",
    AppConstants.MOZ_WIDGET_GTK ? { altKey: true } : { accelKey: true }
  );
  is(
    gBrowser.tabContainer.selectedIndex,
    1,
    "Number shortcut to select the first tab skipped the Firefox View tab"
  );
  closeFirefoxViewTab();
});

add_task(async function accel_w_behavior() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await openFirefoxViewTab(win);
  EventUtils.synthesizeKey("w", { accelKey: true }, win);
  ok(!win.FirefoxViewHandler.tab, "Accel+w closed the Firefox View tab");
  await openFirefoxViewTab(win);
  win.gBrowser.selectedTab = win.gBrowser.visibleTabs[0];
  info(
    "Waiting for Accel+W in the only visible tab to close the window, ignoring the presence of the hidden Firefox View tab"
  );
  let windowClosed = BrowserTestUtils.windowClosed(win);
  EventUtils.synthesizeKey("w", { accelKey: true }, win);
  await windowClosed;
});

add_task(async function undo_close_tab() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Closed tab count after purging session history"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:about"
  );
  await TestUtils.waitForTick();

  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  gBrowser.removeTab(tab);
  await sessionUpdatePromise;
  is(
    SessionStore.getClosedTabCount(window),
    1,
    "Closing about:about added to the closed tab count"
  );

  let viewTab = await openFirefoxViewTab();
  await TestUtils.waitForTick();
  sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(viewTab);
  closeFirefoxViewTab();
  await sessionUpdatePromise;
  is(
    SessionStore.getClosedTabCount(window),
    1,
    "Closing the Firefox View tab did not add to the closed tab count"
  );
});
