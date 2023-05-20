/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the context menu for the search bar.
 */

"use strict";

let win;

XPCOMUtils.defineLazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

add_setup(async function () {
  await gCUITestUtils.addSearchBar();

  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  win = await BrowserTestUtils.openNewBrowserWindow();

  // Disable suggestions for this test, so that we are not attempting to hit
  // the network for suggestions when we don't need them.
  SpecialPowers.pushPrefEnv({
    set: [["browser.search.suggest.enabled", false]],
  });

  registerCleanupFunction(async function () {
    await BrowserTestUtils.closeWindow(win);
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function test_emptybar() {
  const searchbar = win.BrowserSearch.searchBar;
  searchbar.focus();

  let contextMenu = searchbar.querySelector(".textbox-contextmenu");
  let contextMenuPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  await EventUtils.synthesizeMouseAtCenter(
    searchbar,
    { type: "contextmenu", button: 2 },
    win
  );
  await contextMenuPromise;

  Assert.ok(
    contextMenu.getElementsByAttribute("cmd", "cmd_cut")[0].disabled,
    "Should have disabled the cut menuitem"
  );
  Assert.ok(
    contextMenu.getElementsByAttribute("cmd", "cmd_copy")[0].disabled,
    "Should have disabled the copy menuitem"
  );
  Assert.ok(
    contextMenu.getElementsByAttribute("cmd", "cmd_delete")[0].disabled,
    "Should have disabled the delete menuitem"
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
});

add_task(async function test_text_in_bar() {
  const searchbar = win.BrowserSearch.searchBar;
  searchbar.focus();

  searchbar.value = "Test";
  searchbar._textbox.editor.selectAll();

  let contextMenu = searchbar.querySelector(".textbox-contextmenu");
  let contextMenuPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  await EventUtils.synthesizeMouseAtCenter(
    searchbar,
    { type: "contextmenu", button: 2 },
    win
  );
  await contextMenuPromise;

  Assert.ok(
    !contextMenu.getElementsByAttribute("cmd", "cmd_cut")[0].disabled,
    "Should have enabled the cut menuitem"
  );
  Assert.ok(
    !contextMenu.getElementsByAttribute("cmd", "cmd_copy")[0].disabled,
    "Should have enabled the copy menuitem"
  );
  Assert.ok(
    !contextMenu.getElementsByAttribute("cmd", "cmd_delete")[0].disabled,
    "Should have enabled the delete menuitem"
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
});

add_task(async function test_unfocused_emptybar() {
  const searchbar = win.BrowserSearch.searchBar;
  // clear searchbar value from previous test
  searchbar.value = "";

  // force focus onto another component
  win.gURLBar.focus();

  let contextMenu = searchbar.querySelector(".textbox-contextmenu");
  let contextMenuPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  searchbar.focus();
  await EventUtils.synthesizeMouseAtCenter(
    searchbar,
    { type: "contextmenu", button: 2 },
    win
  );
  await contextMenuPromise;

  Assert.ok(
    contextMenu.getElementsByAttribute("cmd", "cmd_cut")[0].disabled,
    "Should have disabled the cut menuitem"
  );
  Assert.ok(
    contextMenu.getElementsByAttribute("cmd", "cmd_copy")[0].disabled,
    "Should have disabled the copy menuitem"
  );
  Assert.ok(
    contextMenu.getElementsByAttribute("cmd", "cmd_delete")[0].disabled,
    "Should have disabled the delete menuitem"
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
});

add_task(async function test_text_in_unfocused_bar() {
  const searchbar = win.BrowserSearch.searchBar;

  searchbar.value = "Test";

  // force focus onto another component
  win.gURLBar.focus();

  let contextMenu = searchbar.querySelector(".textbox-contextmenu");
  let contextMenuPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  searchbar.focus();
  await EventUtils.synthesizeMouseAtCenter(
    searchbar,
    { type: "contextmenu", button: 2 },
    win
  );
  await contextMenuPromise;

  Assert.ok(
    !contextMenu.getElementsByAttribute("cmd", "cmd_cut")[0].disabled,
    "Should have enabled the cut menuitem"
  );
  Assert.ok(
    !contextMenu.getElementsByAttribute("cmd", "cmd_copy")[0].disabled,
    "Should have enabled the copy menuitem"
  );
  Assert.ok(
    !contextMenu.getElementsByAttribute("cmd", "cmd_delete")[0].disabled,
    "Should have enabled the delete menuitem"
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
});

add_task(async function test_paste_and_go() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
  });

  const searchbar = win.BrowserSearch.searchBar;

  searchbar.value = "";
  searchbar.focus();

  const searchString = "test";

  await SimpleTest.promiseClipboardChange(searchString, () => {
    clipboardHelper.copyString(searchString);
  });

  let contextMenu = searchbar.querySelector(".textbox-contextmenu");
  let contextMenuPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await EventUtils.synthesizeMouseAtCenter(
    searchbar,
    { type: "contextmenu", button: 2 },
    win
  );
  await contextMenuPromise;

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  searchbar.querySelector(".searchbar-paste-and-search").click();
  await p;
  contextMenu.hidePopup();
  await popupHiddenPromise;

  Assert.equal(
    tab.linkedBrowser.currentURI.spec,
    `https://example.com/?q=${searchString}`,
    "Should have loaded the expected search page."
  );
});
