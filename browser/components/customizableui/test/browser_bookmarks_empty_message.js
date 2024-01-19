/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function emptyToolbarMessageVisible(visible, win = window) {
  info("Empty toolbar message should be " + (visible ? "visible" : "hidden"));
  let emptyMessage = win.document.getElementById("personal-toolbar-empty");
  await BrowserTestUtils.waitForMutationCondition(
    emptyMessage,
    { attributes: true, attributeFilter: ["hidden"] },
    () => emptyMessage.hidden != visible
  );
}

add_task(async function empty_message_on_non_empty_bookmarks_toolbar() {
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "Default state to begin");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.visibility", "always"]],
  });

  CustomizableUI.removeWidgetFromArea("import-button");
  CustomizableUI.removeWidgetFromArea("personal-bookmarks");
  CustomizableUI.addWidgetToArea(
    "bookmarks-menu-button",
    CustomizableUI.AREA_BOOKMARKS,
    0
  );

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let doc = newWin.document;
  ok(
    BrowserTestUtils.isVisible(doc.getElementById("PersonalToolbar")),
    "Personal toolbar should be visible"
  );
  ok(
    doc.getElementById("personal-toolbar-empty").hidden,
    "Empty message should be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
  await resetCustomization();
});

add_task(async function empty_message_after_customization() {
  // ensure There's something on the toolbar.
  let bm = await PlacesUtils.bookmarks.insert({
    url: "https://mozilla.org/",
    title: "test",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  registerCleanupFunction(() => PlacesUtils.bookmarks.remove(bm));
  ok(CustomizableUI.inDefaultState, "Default state to begin");

  // Open window with a visible toolbar.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.visibility", "always"]],
  });
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let doc = newWin.document;
  let toolbar = doc.getElementById("PersonalToolbar");
  ok(BrowserTestUtils.isVisible(toolbar), "Personal toolbar should be visible");
  await emptyToolbarMessageVisible(false, newWin);

  // Force a Places view uninit through customization.
  CustomizableUI.removeWidgetFromArea("personal-bookmarks");
  await resetCustomization();
  // Show the toolbar again.
  setToolbarVisibility(toolbar, true, false, false);
  ok(BrowserTestUtils.isVisible(toolbar), "Personal toolbar should be visible");
  // Wait for bookmarks to be visible.
  let placesItems = doc.getElementById("PlacesToolbarItems");
  await BrowserTestUtils.waitForMutationCondition(
    placesItems,
    { childList: true },
    () => placesItems.childNodes.length
  );
  await emptyToolbarMessageVisible(false, newWin);

  await BrowserTestUtils.closeWindow(newWin);
});
