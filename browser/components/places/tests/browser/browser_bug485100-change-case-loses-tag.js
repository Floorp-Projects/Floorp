"use strict";

add_task(async function() {
  let testTag = "foo";
  let testTagUpper = "Foo";
  let testURI = Services.io.newURI("http://www.example.com/");

  // Add a bookmark.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    title: "mozilla",
    url: testURI,
  });

  // Init panel
  ok(gEditItemOverlay, "gEditItemOverlay is in context");
  let node = await PlacesUIUtils.promiseNodeLikeFromFetchInfo(bm);
  gEditItemOverlay.initPanel({ node });

  // add a tag
  document.getElementById("editBMPanel_tagsField").value = testTag;
  let promiseNotification = PlacesTestUtils.waitForNotification(
    "onItemChanged",
    (id, property) => property == "tags"
  );
  gEditItemOverlay.onTagsFieldChange();
  await promiseNotification;

  // test that the tag has been added in the backend
  is(PlacesUtils.tagging.getTagsForURI(testURI)[0], testTag, "tags match");

  // change the tag
  document.getElementById("editBMPanel_tagsField").value = testTagUpper;
  // The old sync API doesn't notify a tags change, and fixing it would be
  // quite complex, so we just wait for a title change until tags are
  // refactored.
  promiseNotification = PlacesTestUtils.waitForNotification(
    "onItemChanged",
    (id, property) => property == "title"
  );
  gEditItemOverlay.onTagsFieldChange();
  await promiseNotification;

  // test that the tag has been added in the backend
  is(PlacesUtils.tagging.getTagsForURI(testURI)[0], testTagUpper, "tags match");

  // Cleanup.
  PlacesUtils.tagging.untagURI(testURI, [testTag]);
  await PlacesUtils.bookmarks.remove(bm.guid);
});
