/**
 * Tests that multiple tags can be added to a bookmark using the Library.
 */
"use strict";

add_task(async function test_add_tags() {
  const uri = "http://example.com/";

  // Add a bookmark.
  await PlacesUtils.bookmarks.insert({
    url: uri,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });

  // Open the Library and select the "UnfilledBookmarks".
  let library = await promiseLibrary("UnfiledBookmarks");

  let bookmarkNode = library.ContentTree.view.selectedNode;
  Assert.equal(bookmarkNode.uri, "http://example.com/", "Found the expected bookmark");

  // Add a tag to the bookmark.
  fillBookmarkTextField("editBMPanel_tagsField", "tag1", library);

  await waitForCondition(() => bookmarkNode.tags === "tag1", "Node tag is correct");

  // Add a new tag to the bookmark.
  fillBookmarkTextField("editBMPanel_tagsField", "tag1, tag2", library);

  await waitForCondition(() => bookmarkNode.tags === "tag1, tag2", "Node tag is correct");

  // Check the tag change has been.
  let tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(uri));
  Assert.equal(tags.length, 2, "Found the right number of tags");
  Assert.deepEqual(tags, ["tag1", "tag2"], "Found the expected tags");

  // Cleanup
  registerCleanupFunction(async function() {
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.eraseEverything();
  });
});
