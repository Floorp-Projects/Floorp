/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that we add the import button only if there aren't enough bookmarks
 * in the toolbar.
 */
add_task(async function test_bookmark_import_button() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });
  let bookmarkCount = PlacesUtils.getChildCountForFolder(
    PlacesUtils.bookmarks.toolbarGuid
  );
  Assert.less(bookmarkCount, 3, "we should start with less than 3 bookmarks");

  ok(
    !document.getElementById("import-button"),
    "Shouldn't have button to start with."
  );
  await PlacesUIUtils.maybeAddImportButton();
  ok(document.getElementById("import-button"), "Button should be added.");

  CustomizableUI.reset();

  // Add some bookmarks. This should stop the import button from being inserted.
  let parentGuid = PlacesUtils.bookmarks.toolbarGuid;
  let bookmarks = await Promise.all(
    ["firefox", "rules", "yo"].map(n =>
      PlacesUtils.bookmarks.insert({
        parentGuid,
        url: `https://example.com/${n}`,
        title: n.toString(),
      })
    )
  );
  registerCleanupFunction(async () =>
    Promise.all(bookmarks.map(b => PlacesUtils.bookmarks.remove(b.guid)))
  );

  await PlacesUIUtils.maybeAddImportButton();
  ok(
    !document.getElementById("import-button"),
    "Button should not be added if we have bookmarks."
  );

  // Just in case, for future tests we run:
  CustomizableUI.reset();
});
