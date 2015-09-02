"use strict"

add_task(function* () {
  info("Add a live bookmark editing its data");

  yield withSidebarTree("bookmarks", function* (tree) {
    let itemId = PlacesUIUtils.leftPaneQueries["UnfiledBookmarks"];
    tree.selectItems([itemId]);

    yield withBookmarksDialog(
      true,
      function openDialog() {
        PlacesCommandHook.addLiveBookmark("http://livemark.com/",
                                          "livemark", "description");
      },
      function* test(dialogWin) {
        let promiseTitleChangeNotification = promiseBookmarksNotification(
          "onItemChanged", (itemId, prop, isAnno, val) => prop == "title" && val == "modified");

        fillBookmarkTextField("editBMPanel_namePicker", "modified", dialogWin);

        yield promiseTitleChangeNotification;

        let bookmark = yield PlacesUtils.bookmarks.fetch({
          parentGuid: PlacesUtils.bookmarks.toolbarGuid,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX
        });

        is(bookmark.title, "modified", "folder name has been edited");

        let livemark = yield PlacesUtils.livemarks.getLivemark({
          guid: bookmark.guid
        });
        is(livemark.feedURI.spec, "http://livemark.com/", "livemark has the correct url");
        is(livemark.title, "modified", "livemark has the correct title");
      }
    );
  });
});
