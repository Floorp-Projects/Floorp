/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function bookmarks_toolbar_shown_on_newtab() {
  for (let featureEnabled of [true, false]) {
    info(
      "Testing with the feature " + (featureEnabled ? "enabled" : "disabled")
    );
    await SpecialPowers.pushPrefEnv({
      set: [["browser.toolbars.bookmarks.2h2020", featureEnabled]],
    });
    let newtab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });

    // 1: Test that the toolbar is shown in a newly opened foreground about:newtab
    if (featureEnabled) {
      await waitForBookmarksToolbarVisibility({
        visible: true,
        message: "Toolbar should be visible on newtab if enabled",
      });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar should be visible on newtab if enabled"
    );

    // 2: Test that the toolbar is hidden when the browser is navigated away from newtab
    BrowserTestUtils.loadURI(newtab.linkedBrowser, "https://example.com");
    await BrowserTestUtils.browserLoaded(newtab.linkedBrowser);
    if (featureEnabled) {
      await waitForBookmarksToolbarVisibility({
        visible: false,
        message:
          "Toolbar should not be visible on newtab after example.com is loaded within",
      });
    }
    ok(
      !isBookmarksToolbarVisible(),
      "Toolbar should not be visible on newtab after example.com is loaded within"
    );

    // 3: Re-load about:newtab in the browser for the following tests and confirm toolbar reappears
    BrowserTestUtils.loadURI(newtab.linkedBrowser, "about:newtab");
    await BrowserTestUtils.browserLoaded(newtab.linkedBrowser);
    if (featureEnabled) {
      await waitForBookmarksToolbarVisibility({
        visible: true,
        message: "Toolbar should be visible on newtab",
      });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar should be visible on newtab"
    );

    // 4: Toolbar should get hidden when opening a new tab to example.com
    let example = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "https://example.com",
    });
    await waitForBookmarksToolbarVisibility({
      visible: false,
      message: "Toolbar should be hidden on example.com",
    });

    // 5: Toolbar should become visible when switching tabs to newtab
    await BrowserTestUtils.switchTab(gBrowser, newtab);
    if (featureEnabled) {
      await waitForBookmarksToolbarVisibility({
        visible: true,
        message: "Toolbar is visible with switch to newtab if enabled",
      });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible with switch to newtab if enabled"
    );

    // 6: Toolbar should become hidden when switching tabs to example.com
    await BrowserTestUtils.switchTab(gBrowser, example);
    await waitForBookmarksToolbarVisibility({
      visible: false,
      message: "Toolbar is hidden with switch to example",
    });

    // 7: Similar to #3 above, loading about:newtab in example should show toolbar
    BrowserTestUtils.loadURI(example.linkedBrowser, "about:newtab");
    await BrowserTestUtils.browserLoaded(example.linkedBrowser);
    if (featureEnabled) {
      await waitForBookmarksToolbarVisibility({
        visible: true,
        message: "Toolbar is visible with newtab load if enabled",
      });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible with newtab load if enabled"
    );

    // 8: Switching back and forth between two browsers showing about:newtab will still show the toolbar
    await BrowserTestUtils.switchTab(gBrowser, newtab);
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible with switch to newtab if enabled"
    );
    await BrowserTestUtils.switchTab(gBrowser, example);
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible with switch to example(newtab) if enabled"
    );

    // 9: With custom newtab URL, toolbar isn't shown on about:newtab but is shown on custom URL
    let oldNewTab = AboutNewTab.newTabURL;
    AboutNewTab.newTabURL = "https://example.com/2";
    await BrowserTestUtils.switchTab(gBrowser, newtab);
    await waitForBookmarksToolbarVisibility({ visible: false });
    ok(!isBookmarksToolbarVisible(), "Toolbar should hide with custom newtab");
    BrowserTestUtils.loadURI(example.linkedBrowser, AboutNewTab.newTabURL);
    await BrowserTestUtils.browserLoaded(example.linkedBrowser);
    await BrowserTestUtils.switchTab(gBrowser, example);
    if (featureEnabled) {
      await waitForBookmarksToolbarVisibility({ visible: true });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible with switch to custom newtab if enabled"
    );

    await BrowserTestUtils.removeTab(newtab);
    await BrowserTestUtils.removeTab(example);
    AboutNewTab.newTabURL = oldNewTab;
  }
});

add_task(async function bookmarks_toolbar_open_persisted() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });
  let newtab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:newtab",
    waitForLoad: false,
  });
  let example = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
  });
  let isToolbarPersistedOpen = () =>
    Services.prefs.getCharPref("browser.toolbars.bookmarks.visibility") ==
    "always";

  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForBookmarksToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  await waitForBookmarksToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");
  ok(!isToolbarPersistedOpen(), "Toolbar is not persisted open");

  let contextMenu = document.querySelector("#toolbar-context-menu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let menuButton = document.getElementById("PanelUI-menu-button");
  EventUtils.synthesizeMouseAtCenter(
    menuButton,
    { type: "contextmenu" },
    window
  );
  await popupShown;
  let bookmarksToolbarMenu = document.querySelector("#toggle_PersonalToolbar");
  let subMenu = bookmarksToolbarMenu.querySelector("menupopup");
  popupShown = BrowserTestUtils.waitForEvent(subMenu, "popupshown");
  bookmarksToolbarMenu.openMenu(true);
  await popupShown;
  let alwaysMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="always"]'
  );
  let neverMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="never"]'
  );
  let newTabMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="newtab"]'
  );
  is(alwaysMenuItem.getAttribute("checked"), "false", "Menuitem isn't checked");
  is(neverMenuItem.getAttribute("checked"), "false", "Menuitem isn't checked");
  is(newTabMenuItem.getAttribute("checked"), "true", "Menuitem is checked");

  subMenu.activateItem(alwaysMenuItem);

  await waitForBookmarksToolbarVisibility({ visible: true });
  popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    menuButton,
    { type: "contextmenu" },
    window
  );
  await popupShown;
  bookmarksToolbarMenu = document.querySelector("#toggle_PersonalToolbar");
  subMenu = bookmarksToolbarMenu.querySelector("menupopup");
  popupShown = BrowserTestUtils.waitForEvent(subMenu, "popupshown");
  bookmarksToolbarMenu.openMenu(true);
  await popupShown;
  alwaysMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="always"]'
  );
  neverMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="never"]'
  );
  newTabMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="newtab"]'
  );
  is(alwaysMenuItem.getAttribute("checked"), "true", "Menuitem is checked");
  is(neverMenuItem.getAttribute("checked"), "false", "Menuitem isn't checked");
  is(newTabMenuItem.getAttribute("checked"), "false", "Menuitem isn't checked");
  contextMenu.hidePopup();
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  ok(isToolbarPersistedOpen(), "Toolbar is persisted open");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");

  popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    menuButton,
    { type: "contextmenu" },
    window
  );
  await popupShown;
  bookmarksToolbarMenu = document.querySelector("#toggle_PersonalToolbar");
  subMenu = bookmarksToolbarMenu.querySelector("menupopup");
  popupShown = BrowserTestUtils.waitForEvent(subMenu, "popupshown");
  bookmarksToolbarMenu.openMenu(true);
  await popupShown;
  alwaysMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="always"]'
  );
  neverMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="never"]'
  );
  newTabMenuItem = document.querySelector(
    'menuitem[data-visibility-enum="newtab"]'
  );
  is(alwaysMenuItem.getAttribute("checked"), "true", "Menuitem is checked");
  is(neverMenuItem.getAttribute("checked"), "false", "Menuitem isn't checked");
  is(newTabMenuItem.getAttribute("checked"), "false", "Menuitem isn't checked");
  subMenu.activateItem(newTabMenuItem);
  await waitForBookmarksToolbarVisibility({
    visible: false,
    message: "Toolbar is hidden",
  });
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForBookmarksToolbarVisibility({
    visible: true,
    message: "Toolbar is visible",
  });
  await BrowserTestUtils.switchTab(gBrowser, example);
  await waitForBookmarksToolbarVisibility({
    visible: false,
    message: "Toolbar is hidden",
  });

  await BrowserTestUtils.removeTab(newtab);
  await BrowserTestUtils.removeTab(example);
});

add_task(async function test_with_newtabpage_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.toolbars.bookmarks.2h2020", true],
      ["browser.newtabpage.enabled", true],
    ],
  });

  let tabCount = gBrowser.tabs.length;
  document.getElementById("cmd_newNavigatorTab").doCommand();
  // Can't use BrowserTestUtils.waitForNewTab since onLocationChange will not
  // fire due to preloaded new tabs.
  await TestUtils.waitForCondition(() => gBrowser.tabs.length == tabCount + 1);
  let newtab = gBrowser.selectedTab;
  is(newtab.linkedBrowser.currentURI.spec, "about:newtab", "newtab is loaded");
  await waitForBookmarksToolbarVisibility({
    visible: true,
    message: "Toolbar is visible with NTP enabled",
  });
  await BrowserTestUtils.removeTab(newtab);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.enabled", false]],
  });

  document.getElementById("cmd_newNavigatorTab").doCommand();
  await TestUtils.waitForCondition(() => gBrowser.tabs.length == tabCount + 1);
  newtab = gBrowser.selectedTab;
  is(newtab.linkedBrowser.currentURI.spec, "about:blank", "blank is loaded");
  await waitForBookmarksToolbarVisibility({
    visible: false,
    message: "Toolbar is not visible with NTP disabled",
  });
  await BrowserTestUtils.removeTab(newtab);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.enabled", true]],
  });
});

add_task(async function test_history_pushstate() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });
  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    await waitForBookmarksToolbarVisibility({ visible: false });
    ok(!isBookmarksToolbarVisible(), "Toolbar should be hidden");

    // Temporarily show the toolbar:
    setToolbarVisibility(
      document.querySelector("#PersonalToolbar"),
      true,
      false,
      false
    );
    ok(isBookmarksToolbarVisible(), "Toolbar should now be visible");

    // Now "navigate"
    await SpecialPowers.spawn(browser, [], () => {
      content.location.href += "#foo";
    });

    await TestUtils.waitForCondition(
      () => gURLBar.value.endsWith("#foo"),
      "URL bar should update"
    );
    ok(isBookmarksToolbarVisible(), "Toolbar should still be visible");
  });
});
