"use strict";

add_task(async function() {
  info("Add a live bookmark editing its data");

  await withSidebarTree("bookmarks", async function(tree) {
    tree.selectItems([PlacesUtils.bookmarks.unfiledGuid]);

    await withBookmarksDialog(
      true,
      function openDialog() {
        PlacesCommandHook.addLiveBookmark("http://livemark.com/",
                                          "livemark", "description");
      },
      async function test(dialogWin) {
        let promiseTitleChangeNotification = PlacesTestUtils.waitForNotification(
          "onItemChanged", (unused, prop, isAnno, val) => prop == "title" && val == "modified");

        fillBookmarkTextField("editBMPanel_namePicker", "modified", dialogWin);

        await promiseTitleChangeNotification;

        let bookmark = await PlacesUtils.bookmarks.fetch({
          parentGuid: PlacesUtils.bookmarks.toolbarGuid,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX
        });

        Assert.equal(bookmark.title, "modified", "folder name has been edited");

        let livemark = await PlacesUtils.livemarks.getLivemark({
          guid: bookmark.guid
        });
        Assert.equal(livemark.feedURI.spec, "http://livemark.com/", "livemark has the correct url");
        Assert.equal(livemark.title, "modified", "livemark has the correct title");
      }
    );
  });
});
