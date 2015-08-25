"use strict"

add_task(function* () {
  info("Bug 479348 - Properties on a root should be read-only.");

  yield withSidebarTree("bookmarks", function* (tree) {
    let itemId = PlacesUIUtils.leftPaneQueries["UnfiledBookmarks"];
    tree.selectItems([itemId]);
    ok(tree.controller.isCommandEnabled("placesCmd_show:info"),
       "'placesCmd_show:info' on current selected node is enabled");

    yield withBookmarksDialog(
      true,
      function openDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      function* test(dialogWin) {
        // Check that the dialog is read-only.
        ok(dialogWin.gEditItemOverlay.readOnly, "Dialog is read-only");
        // Check that accept button is disabled
        let acceptButton = dialogWin.document.documentElement.getButton("accept");
        ok(acceptButton.disabled, "Accept button is disabled");

        // Check that name picker is read only
        let namepicker = dialogWin.document.getElementById("editBMPanel_namePicker");
        ok(namepicker.readOnly, "Name field is read-only");
        is(namepicker.value,
           PlacesUtils.bookmarks.getItemTitle(PlacesUtils.unfiledBookmarksFolderId),
           "Node title is correct");
        // Blur the field and ensure root's name has not been changed.
        namepicker.blur();
        is(namepicker.value,
           PlacesUtils.bookmarks.getItemTitle(PlacesUtils.unfiledBookmarksFolderId),
           "Root title is correct");
        // Check the shortcut's title.
        let bookmark = yield PlacesUtils.bookmarks.fetch(tree.selectedNode.bookmarkGuid);
        is(bookmark.title, null,
           "Shortcut title is null");
      }
    );
  });
});
