"use strict"

add_task(function* () {
  info("Bug 475529 - Add is the default button for the new folder dialog + " +
       "Bug 1206376 - Changing properties of a new bookmark while adding it " +
       "acts on the last bookmark in the current container");

  // Add a new bookmark at index 0 in the unfiled folder.
  let insertionIndex = 0;
  let newBookmark = yield PlacesUtils.bookmarks.insert({
    index: insertionIndex,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/",
  });
  let newBookmarkId = yield PlacesUtils.promiseItemId(newBookmark.guid);

  yield withSidebarTree("bookmarks", function* (tree) {
    // Select the new bookmark in the sidebar.
    tree.selectItems([newBookmarkId]);
    ok(tree.controller.isCommandEnabled("placesCmd_new:folder"),
       "'placesCmd_new:folder' on current selected node is enabled");

    // Create a new folder.  Since the new bookmark is selected, and new items
    // are inserted at the index of the currently selected item, the new folder
    // will be inserted at index 0.
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

        let newFolder = yield PlacesUtils.bookmarks.fetch({
          parentGuid: PlacesUtils.bookmarks.unfiledGuid,
          index: insertionIndex,
        });

        is(newFolder.title, "n", "folder name has been edited");
        yield PlacesUtils.bookmarks.remove(newFolder);
        yield PlacesUtils.bookmarks.remove(newBookmark);
      }
    );
  });
});
