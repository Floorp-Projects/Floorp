"use strict"

add_task(function* () {
  info("Bug 475529 - Add is the default button for the new folder dialog.");

  yield withSidebarTree("bookmarks", function* (tree) {
    let itemId = PlacesUIUtils.leftPaneQueries["UnfiledBookmarks"];
    tree.selectItems([itemId]);
    ok(tree.controller.isCommandEnabled("placesCmd_new:folder"),
       "'placesCmd_new:folder' on current selected node is enabled");

    yield withBookmarksDialog(
      false,
      function openDialog() {
        tree.controller.doCommand("placesCmd_new:folder");
      },
      function* test(dialogWin) {
        let promiseTitleChangeNotification = promiseBookmarksNotification(
          "onItemChanged", (itemId, prop, isAnno, val) => prop == "title" && val =="n");

        fillBookmarkTextField("editBMPanel_namePicker", "n", dialogWin, false);

        // Confirm and close the dialog.
        EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
        yield promiseTitleChangeNotification;

        let bookmark = yield PlacesUtils.bookmarks.fetch({
          parentGuid: PlacesUtils.bookmarks.unfiledGuid,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX
        });

        is(bookmark.title, "n", "folder name has been edited");
        yield PlacesUtils.bookmarks.remove(bookmark);
      }
    );
  });
});
