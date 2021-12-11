/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "http://example.com/";

add_task(async function() {
  await PlacesUtils.bookmarks.eraseEverything();
  let organizer = await promiseLibrary();

  registerCleanupFunction(async function() {
    await promiseLibraryClosed(organizer);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  let PlacesOrganizer = organizer.PlacesOrganizer;
  let ContentTree = organizer.ContentTree;

  // Test with multiple entries to ensure they retain their order.
  let bookmarks = [];
  bookmarks.push(
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: TEST_URL,
      title: "0",
    })
  );
  bookmarks.push(
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: TEST_URL,
      title: "1",
    })
  );
  bookmarks.push(
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: TEST_URL,
      title: "2",
    })
  );

  await selectBookmarksIn(organizer, bookmarks, "BookmarksToolbar");

  await promiseClipboard(() => {
    info("Cutting selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  await selectBookmarksIn(organizer, bookmarks, "UnfiledBookmarks");
});

var selectBookmarksIn = async function(organizer, bookmarks, aLeftPaneQuery) {
  let PlacesOrganizer = organizer.PlacesOrganizer;
  let ContentTree = organizer.ContentTree;
  info("Selecting " + aLeftPaneQuery + " in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn(aLeftPaneQuery);

  for (let { guid } of bookmarks) {
    let bookmark = await PlacesUtils.bookmarks.fetch(guid);
    is(
      bookmark.parentGuid,
      PlacesUtils.getConcreteItemGuid(PlacesOrganizer._places.selectedNode),
      "Bookmark has the right parent"
    );
  }

  info("Selecting the bookmarks in the right pane");
  ContentTree.view.selectItems(bookmarks.map(bm => bm.guid));

  for (let node of ContentTree.view.selectedNodes) {
    is(
      "" + node.bookmarkIndex,
      node.title,
      "Found the expected bookmark in the expected position"
    );
  }
};
