/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function openContextMenu() {
  let contextMenu = document.getElementById("tabContextMenu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {
    type: "contextmenu",
    button: 2,
  });
  await popupShown;
}

async function closeContextMenu() {
  let contextMenu = document.getElementById("tabContextMenu");
  let popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHidden;
}

add_task(async function test() {
  if (Services.prefs.getBoolPref("widget.macos.native-context-menus", false)) {
    ok(
      true,
      "This bug is not possible when native context menus are enabled on macOS."
    );
    return;
  }
  // Ensure tabs are focusable.
  await SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });

  // There should be one tab when we start the test.
  let tab1 = gBrowser.selectedTab;
  let tab2 = BrowserTestUtils.addTab(gBrowser);
  tab1.focus();
  is(document.activeElement, tab1, "tab1 should be focused");

  // Ensure that DownArrow doesn't switch to tab2 while the context menu is open.
  await openContextMenu();
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await closeContextMenu();
  is(gBrowser.selectedTab, tab1, "tab1 should still be active");
  if (AppConstants.platform == "macosx") {
    // On Mac, focus doesn't return to the tab after dismissing the context menu.
    // Since we're not testing that here, work around it by just focusing again.
    tab1.focus();
  }
  is(document.activeElement, tab1, "tab1 should be focused");

  // Switch to tab2 by pressing DownArrow.
  await BrowserTestUtils.switchTab(gBrowser, () =>
    EventUtils.synthesizeKey("KEY_ArrowDown")
  );
  is(gBrowser.selectedTab, tab2, "should have switched to tab2");
  is(document.activeElement, tab2, "tab2 should now be focused");
  // Ensure that UpArrow doesn't switch to tab1 while the context menu is open.
  await openContextMenu();
  EventUtils.synthesizeKey("KEY_ArrowUp");
  await closeContextMenu();
  is(gBrowser.selectedTab, tab2, "tab2 should still be active");

  gBrowser.removeTab(tab2);
});
