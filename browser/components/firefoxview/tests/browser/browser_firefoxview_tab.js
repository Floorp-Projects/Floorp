/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function expectFocusAfterKey(
  aKey,
  aFocus,
  aAncestorOk = false,
  aWindow = window
) {
  let res = aKey.match(/^(Shift\+)?(?:(.)|(.+))$/);
  let shift = Boolean(res[1]);
  let key;
  if (res[2]) {
    key = res[2]; // Character.
  } else {
    key = "KEY_" + res[3]; // Tab, ArrowRight, etc.
  }
  let expected;
  let friendlyExpected;
  if (typeof aFocus == "string") {
    expected = aWindow.document.getElementById(aFocus);
    friendlyExpected = aFocus;
  } else {
    expected = aFocus;
    if (aFocus == aWindow.gURLBar.inputField) {
      friendlyExpected = "URL bar input";
    } else if (aFocus == aWindow.gBrowser.selectedBrowser) {
      friendlyExpected = "Web document";
    }
  }
  info("Listening on item " + (expected.id || expected.className));
  let focused = BrowserTestUtils.waitForEvent(expected, "focus", aAncestorOk);
  EventUtils.synthesizeKey(key, { shiftKey: shift }, aWindow);
  let receivedEvent = await focused;
  info(
    "Got focus on item: " +
      (receivedEvent.target.id || receivedEvent.target.className)
  );
  ok(true, friendlyExpected + " focused after " + aKey + " pressed");
}

function forceFocus(aElem) {
  aElem.setAttribute("tabindex", "-1");
  aElem.focus();
  aElem.removeAttribute("tabindex");
}

function triggerClickOn(target, options) {
  let promise = BrowserTestUtils.waitForEvent(target, "click");
  if (AppConstants.platform == "macosx") {
    options.metaKey = options.ctrlKey;
    delete options.ctrlKey;
  }
  EventUtils.synthesizeMouseAtCenter(target, options);
  return promise;
}

async function add_new_tab(URL) {
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}

add_task(async function aria_attributes() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  is(
    win.FirefoxViewHandler.button.getAttribute("role"),
    "button",
    "Firefox View button should have the 'button' ARIA role"
  );
  await openFirefoxViewTab(win);
  isnot(
    win.FirefoxViewHandler.button.getAttribute("aria-controls"),
    "",
    "Firefox View button should have non-empty `aria-controls` attribute"
  );
  is(
    win.FirefoxViewHandler.button.getAttribute("aria-controls"),
    win.FirefoxViewHandler.tab.linkedPanel,
    "Firefox View button should refence the hidden tab's linked panel via `aria-controls`"
  );
  is(
    win.FirefoxViewHandler.button.getAttribute("aria-pressed"),
    "true",
    'Firefox View button should have `aria-pressed="true"` upon selecting it'
  );
  win.BrowserOpenTab();
  is(
    win.FirefoxViewHandler.button.getAttribute("aria-pressed"),
    "false",
    'Firefox View button should have `aria-pressed="false"` upon selecting a different tab'
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function load_opens_new_tab() {
  await withFirefoxView({ openNewWindow: true }, async browser => {
    let win = browser.ownerGlobal;
    ok(win.FirefoxViewHandler.tab.selected, "Firefox View tab is selected");
    win.gURLBar.focus();
    win.gURLBar.value = "https://example.com";
    let newTabOpened = BrowserTestUtils.waitForEvent(
      win.gBrowser.tabContainer,
      "TabOpen"
    );
    EventUtils.synthesizeKey("KEY_Enter", {}, win);
    info(
      "Waiting for new tab to open from the address bar in the Firefox View tab"
    );
    await newTabOpened;
    assertFirefoxViewTab(win);
    ok(
      !win.FirefoxViewHandler.tab.selected,
      "Firefox View tab is not selected anymore (new tab opened in the foreground)"
    );
  });
});

add_task(async function homepage_new_tab() {
  await withFirefoxView({ openNewWindow: true }, async browser => {
    let win = browser.ownerGlobal;
    ok(win.FirefoxViewHandler.tab.selected, "Firefox View tab is selected");
    let newTabOpened = BrowserTestUtils.waitForEvent(
      win.gBrowser.tabContainer,
      "TabOpen"
    );
    win.BrowserHome();
    info("Waiting for BrowserHome() to open a new tab");
    await newTabOpened;
    assertFirefoxViewTab(win);
    ok(
      !win.FirefoxViewHandler.tab.selected,
      "Firefox View tab is not selected anymore (home page opened in the foreground)"
    );
  });
});

add_task(async function number_tab_select_shortcut() {
  await withFirefoxView({}, async browser => {
    let win = browser.ownerGlobal;
    EventUtils.synthesizeKey(
      "1",
      AppConstants.MOZ_WIDGET_GTK ? { altKey: true } : { accelKey: true },
      win
    );
    ok(
      !win.FirefoxViewHandler.tab.selected,
      "Number shortcut to select the first tab skipped the Firefox View tab"
    );
  });
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
  let win = await BrowserTestUtils.openNewBrowserWindow();
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCountForWindow(win),
    0,
    "Closed tab count after purging session history"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    "about:about"
  );
  await TestUtils.waitForTick();

  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  win.gBrowser.removeTab(tab);
  await sessionUpdatePromise;
  is(
    SessionStore.getClosedTabCountForWindow(win),
    1,
    "Closing about:about added to the closed tab count"
  );

  let viewTab = await openFirefoxViewTab(win);
  await TestUtils.waitForTick();
  sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(viewTab);
  closeFirefoxViewTab(win);
  await sessionUpdatePromise;
  is(
    SessionStore.getClosedTabCountForWindow(win),
    1,
    "Closing the Firefox View tab did not add to the closed tab count"
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_firefoxview_view_count() {
  const startViews = 2;
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.view-count", startViews]],
  });

  let tab = await openFirefoxViewTab(window);

  Assert.strictEqual(
    SpecialPowers.getIntPref("browser.firefox-view.view-count"),
    startViews + 1,
    "View count pref value is incremented when tab is selected"
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_add_ons_cant_unhide_fx_view() {
  // Test that add-ons can't unhide the Firefox View tab by calling
  // browser.tabs.show(). See bug 1791770 for details.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    "about:about"
  );
  let viewTab = await openFirefoxViewTab(win);
  win.gBrowser.hideTab(tab);

  ok(tab.hidden, "Regular tab is hidden");
  ok(viewTab.hidden, "Firefox View tab is hidden");

  win.gBrowser.showTab(tab);
  win.gBrowser.showTab(viewTab);

  ok(!tab.hidden, "Add-on showed regular hidden tab");
  ok(viewTab.hidden, "Add-on did not show Firefox View tab");

  await BrowserTestUtils.closeWindow(win);
});

// Test navigation to first visible tab when the
// Firefox View button is present and active.
add_task(async function testFirstTabFocusableWhenFxViewOpen() {
  await withFirefoxView({}, async browser => {
    let win = browser.ownerGlobal;
    ok(win.FirefoxViewHandler.tab.selected, "Firefox View tab is selected");
    let fxViewBtn = win.document.getElementById("firefox-view-button");
    forceFocus(fxViewBtn);
    is(
      win.document.activeElement,
      fxViewBtn,
      "Firefox View button focused for start of test"
    );
    let firstVisibleTab = win.gBrowser.visibleTabs[0];
    await expectFocusAfterKey("Tab", firstVisibleTab, false, win);
    let activeElement = win.document.activeElement;
    let expectedElement = firstVisibleTab;
    is(activeElement, expectedElement, "First visible tab should be focused");
  });
});

// Test that Firefox View tab is not multiselectable
add_task(async function testFxViewNotMultiselect() {
  await withFirefoxView({}, async browser => {
    let win = browser.ownerGlobal;
    Assert.ok(
      win.FirefoxViewHandler.tab.selected,
      "Firefox View tab is selected"
    );
    let tab2 = await add_new_tab("https://www.mozilla.org");
    let fxViewBtn = win.document.getElementById("firefox-view-button");

    info("We multi-select a visible tab with ctrl key down");
    await triggerClickOn(tab2, { ctrlKey: true });
    Assert.ok(
      tab2.multiselected && gBrowser._multiSelectedTabsSet.has(tab2),
      "Second visible tab is (multi) selected"
    );
    Assert.equal(gBrowser.multiSelectedTabsCount, 1, "One tab is selected.");
    Assert.notEqual(
      fxViewBtn,
      gBrowser.selectedTab,
      "Fx View tab doesn't have focus"
    );

    // Ctrl/Cmd click tab2 again to deselect it
    await triggerClickOn(tab2, { ctrlKey: true });

    info("We multi-select visible tabs with shift key down");
    await triggerClickOn(tab2, { shiftKey: true });
    Assert.ok(
      tab2.multiselected && gBrowser._multiSelectedTabsSet.has(tab2),
      "Second visible tab is (multi) selected"
    );
    Assert.equal(gBrowser.multiSelectedTabsCount, 2, "Two tabs are selected.");
    Assert.notEqual(
      fxViewBtn,
      gBrowser.selectedTab,
      "Fx View tab doesn't have focus"
    );

    BrowserTestUtils.removeTab(tab2);
  });
});

add_task(async function testFxViewEntryPointsInPrivateBrowsing() {
  async function checkMenu(win, expectedEnabled) {
    await SimpleTest.promiseFocus(win);
    const toolsMenu = win.document.getElementById("tools-menu");
    const fxViewMenuItem = toolsMenu.querySelector("#menu_openFirefoxView");
    const menuShown = BrowserTestUtils.waitForEvent(toolsMenu, "popupshown");

    toolsMenu.openMenu(true);
    await menuShown;
    Assert.equal(
      BrowserTestUtils.isVisible(fxViewMenuItem),
      expectedEnabled,
      `Firefox view menu item is ${expectedEnabled ? "enabled" : "hidden"}`
    );
    const menuHidden = BrowserTestUtils.waitForEvent(toolsMenu, "popuphidden");
    toolsMenu.menupopup.hidePopup();
    await menuHidden;
  }

  async function checkEntryPointsInWindow(win, expectedVisible) {
    const fxViewBtn = win.document.getElementById("firefox-view-button");

    if (AppConstants.platform != "macosx") {
      await checkMenu(win, expectedVisible);
    }
    // check the tab button
    Assert.equal(
      BrowserTestUtils.isVisible(fxViewBtn),
      expectedVisible,
      `#${fxViewBtn.id} is ${
        expectedVisible ? "visible" : "hidden"
      } as expected`
    );
  }

  info("Check permanent private browsing");
  // Setting permanent private browsing normally requires a restart.
  // We'll emulate by manually setting the attribute it controls manually
  await SpecialPowers.pushPrefEnv({
    set: [["browser.privatebrowsing.autostart", true]],
  });
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  newWin.document.documentElement.setAttribute(
    "privatebrowsingmode",
    "permanent"
  );
  await checkEntryPointsInWindow(newWin, false);
  await BrowserTestUtils.closeWindow(newWin);
  await SpecialPowers.popPrefEnv();

  info("Check defaults (non-private)");
  await SimpleTest.promiseFocus(window);
  await checkEntryPointsInWindow(window, true);

  info("Check private (temporary) browsing");
  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await checkEntryPointsInWindow(privateWin, false);
  await BrowserTestUtils.closeWindow(privateWin);
});
