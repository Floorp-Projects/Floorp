"use strict"

add_task(async function() {
  info("Add a live bookmark editing its data");

  await withSidebarTree("bookmarks", async function(tree) {
    let itemId = PlacesUIUtils.leftPaneQueries["UnfiledBookmarks"];
    tree.selectItems([itemId]);

    await withBookmarksDialog(
      true,
      function openDialog() {
        PlacesCommandHook.addLiveBookmark("http://livemark.com/",
                                          "livemark", "description");
      },
      async function test(dialogWin) {
        let promiseTitleChangeNotification = promiseBookmarksNotification(
          "onItemChanged", (unused, prop, isAnno, val) => prop == "title" && val == "modified");

        fillBookmarkTextField("editBMPanel_namePicker", "modified", dialogWin);

        await promiseTitleChangeNotification;

        let bookmark = await PlacesUtils.bookmarks.fetch({
          parentGuid: PlacesUtils.bookmarks.toolbarGuid,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX
        });

        is(bookmark.title, "modified", "folder name has been edited");

        let livemark = await PlacesUtils.livemarks.getLivemark({
          guid: bookmark.guid
        });
        is(livemark.feedURI.spec, "http://livemark.com/", "livemark has the correct url");
        is(livemark.title, "modified", "livemark has the correct title");
      }
    );
  });
});
