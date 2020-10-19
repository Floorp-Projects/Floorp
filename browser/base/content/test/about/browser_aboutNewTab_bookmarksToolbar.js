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
      await waitForToolbarVisibility({ visible: true });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar should be visible on newtab if enabled"
    );

    // 2: Test that the toolbar is hidden when the browser is navigated away from newtab
    await BrowserTestUtils.loadURI(newtab.linkedBrowser, "https://example.com");
    if (featureEnabled) {
      await waitForToolbarVisibility({ visible: false });
    }
    ok(
      !isBookmarksToolbarVisible(),
      "Toolbar should not be visible on newtab after example.com is loaded within"
    );

    // 3: Re-load about:newtab in the browser for the following tests and confirm toolbar reappears
    await BrowserTestUtils.loadURI(newtab.linkedBrowser, "about:newtab");
    if (featureEnabled) {
      await waitForToolbarVisibility({ visible: true });
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
    await waitForToolbarVisibility({ visible: false });
    ok(!isBookmarksToolbarVisible(), "Toolbar should be hidden on example.com");

    // 5: Toolbar should become visible when switching tabs to newtab
    await BrowserTestUtils.switchTab(gBrowser, newtab);
    if (featureEnabled) {
      await waitForToolbarVisibility({ visible: true });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible with switch to newtab if enabled"
    );

    // 6: Toolbar should become hidden when switching tabs to example.com
    await BrowserTestUtils.switchTab(gBrowser, example);
    await waitForToolbarVisibility({ visible: false });
    ok(
      !isBookmarksToolbarVisible(),
      "Toolbar is hidden with switch to example"
    );

    // 7: Similar to #3 above, loading about:newtab in example should show toolbar
    await BrowserTestUtils.loadURI(example.linkedBrowser, "about:newtab");
    if (featureEnabled) {
      await waitForToolbarVisibility({ visible: true });
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
    await waitForToolbarVisibility({ visible: false });
    ok(!isBookmarksToolbarVisible(), "Toolbar should hide with custom newtab");
    await BrowserTestUtils.loadURI(
      example.linkedBrowser,
      AboutNewTab.newTabURL
    );
    await BrowserTestUtils.switchTab(gBrowser, example);
    if (featureEnabled) {
      await waitForToolbarVisibility({ visible: true });
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
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  await waitForToolbarVisibility({ visible: false });
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
  EventUtils.synthesizeMouseAtCenter(bookmarksToolbarMenu, {});
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

  EventUtils.synthesizeMouseAtCenter(alwaysMenuItem, {});

  await waitForToolbarVisibility({ visible: true });
  popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    menuButton,
    { type: "contextmenu" },
    window
  );
  await popupShown;
  bookmarksToolbarMenu = document.querySelector("#toggle_PersonalToolbar");
  EventUtils.synthesizeMouseAtCenter(bookmarksToolbarMenu, {});
  subMenu = bookmarksToolbarMenu.querySelector("menupopup");
  popupShown = BrowserTestUtils.waitForEvent(subMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(bookmarksToolbarMenu, {});
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
  EventUtils.synthesizeMouseAtCenter(bookmarksToolbarMenu, {});
  subMenu = bookmarksToolbarMenu.querySelector("menupopup");
  popupShown = BrowserTestUtils.waitForEvent(subMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(bookmarksToolbarMenu, {});
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
  EventUtils.synthesizeMouseAtCenter(newTabMenuItem, {});
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");

  await BrowserTestUtils.removeTab(newtab);
  await BrowserTestUtils.removeTab(example);
});

add_task(async function test_with_newtabpage_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });
  let newtab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:newtab",
    waitForLoad: false,
  });
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");

  let blank = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:blank",
    waitForLoad: false,
  });
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");

  let example = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
  });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.enabled", false]],
  });
  await BrowserTestUtils.switchTab(gBrowser, blank);
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, blank);
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");

  await BrowserTestUtils.removeTab(newtab);
  await BrowserTestUtils.removeTab(blank);
  await BrowserTestUtils.removeTab(example);
});

add_task(async function test_with_different_pref_states() {
  // [prefName, prefValue, toolbarVisibleExampleCom, toolbarVisibleNewTab]
  let bookmarksFeatureStates = [
    ["browser.toolbars.bookmarks.2h2020", true],
    ["browser.toolbars.bookmarks.2h2020", false],
  ];
  let bookmarksToolbarVisibilityStates = [
    ["browser.toolbars.bookmarks.visibility", "newtab"],
    ["browser.toolbars.bookmarks.visibility", "always"],
    ["browser.toolbars.bookmarks.visibility", "never"],
  ];
  let newTabEnabledStates = [
    ["browser.newtabpage.enabled", true],
    ["browser.newtabpage.enabled", false],
  ];

  for (let featureState of bookmarksFeatureStates) {
    for (let visibilityState of bookmarksToolbarVisibilityStates) {
      for (let newTabEnabledState of newTabEnabledStates) {
        await SpecialPowers.pushPrefEnv({
          set: [featureState, visibilityState, newTabEnabledState],
        });

        for (let privateWin of [true, false]) {
          info(
            `Testing with ${featureState}, ${visibilityState}, and ${newTabEnabledState} in a ${
              privateWin ? "private" : "non-private"
            } window`
          );
          let win = await BrowserTestUtils.openNewBrowserWindow({
            private: privateWin,
          });
          is(
            win.gBrowser.currentURI.spec,
            privateWin ? "about:privatebrowsing" : "about:blank",
            "Expecting about:privatebrowsing or about:blank as URI of new window"
          );

          if (!privateWin) {
            is(
              isBookmarksToolbarVisible(win),
              visibilityState[1] == "always" ||
                (!newTabEnabledState[1] &&
                  visibilityState[1] == "newtab" &&
                  featureState[1]),
              "Toolbar should be visible only if visibilityState is 'always'. State: " +
                visibilityState[1]
            );
            await BrowserTestUtils.openNewForegroundTab({
              gBrowser: win.gBrowser,
              opening: "about:newtab",
              waitForLoad: false,
            });
          }

          if (featureState[1]) {
            is(
              isBookmarksToolbarVisible(win),
              visibilityState[1] == "newtab" || visibilityState[1] == "always",
              "Toolbar should be visible as long as visibilityState isn't set to 'never'. State: " +
                visibilityState[1]
            );
          } else {
            is(
              isBookmarksToolbarVisible(win),
              visibilityState[1] == "always",
              "Toolbar should be visible only if visibilityState is 'always'. State: " +
                visibilityState[1]
            );
          }
          await BrowserTestUtils.openNewForegroundTab({
            gBrowser: win.gBrowser,
            opening: "http://example.com",
          });
          is(
            isBookmarksToolbarVisible(win),
            visibilityState[1] == "always",
            "Toolbar should be visible only if visibilityState is 'always'. State: " +
              visibilityState[1]
          );
          await BrowserTestUtils.closeWindow(win);
        }
      }
    }
  }
});

async function waitForToolbarVisibility({ visible }) {
  return TestUtils.waitForCondition(() => {
    let toolbar = gNavToolbox.querySelector("#PersonalToolbar");
    return visible ? !toolbar.collapsed : toolbar.collapsed;
  }, "waiting for toolbar to become " + (visible ? "visible" : "hidden"));
}

function isBookmarksToolbarVisible(win = window) {
  let toolbar = win.gNavToolbox.querySelector("#PersonalToolbar");
  return !toolbar.collapsed;
}
