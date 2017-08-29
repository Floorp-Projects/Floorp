"use strict";

let {IsLivemark} = Cu.import("resource:///modules/PlacesUIUtils.jsm", {});

add_task(function test_livemark_cache_builtin_folder() {
  // This test checks a basic livemark, and also initializes the observer for
  // updates to the bookmarks.
  Assert.ok(!IsLivemark(PlacesUtils.unfiledBookmarksFolderId),
    "unfiled bookmarks should not be seen as a livemark");
});

add_task(async function test_livemark_add_and_remove_items() {
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Grandsire",
    url: "http://example.com",
  });

  let bookmarkId = await PlacesUtils.promiseItemId(bookmark.guid);

  Assert.ok(!IsLivemark(bookmarkId),
    "a simple bookmark should not be seen as a livemark");

  let livemark = await PlacesUtils.livemarks.addLivemark({
    title: "Stedman",
    feedURI: Services.io.newURI("http://livemark.com/"),
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  let livemarkId = await PlacesUtils.promiseItemId(livemark.guid);

  Assert.ok(IsLivemark(livemarkId),
    "a livemark should be reported as a livemark");

  // Now remove the livemark.
  await PlacesUtils.livemarks.removeLivemark(livemark);

  Assert.ok(!IsLivemark(livemarkId),
    "the livemark should have been removed from the cache");
});
