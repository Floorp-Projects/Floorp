/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
    BrowserTestUtils.is_visible(doc.getElementById("PersonalToolbar")),
    "Personal toolbar should be visible"
  );
  ok(
    doc.getElementById("personal-toolbar-empty").hidden,
    "Empty message should be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
  await resetCustomization();
});
