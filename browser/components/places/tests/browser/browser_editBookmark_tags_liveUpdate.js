"use strict";

async function checkTagsSelector(aAvailableTags, aCheckedTags) {
  let tags = await PlacesUtils.bookmarks.fetchTags();
  is(tags.length, aAvailableTags.length, "Check tags list");
  let tagsSelector = document.getElementById("editBMPanel_tagsSelector");
  let children = tagsSelector.children;
  is(
    children.length,
    aAvailableTags.length,
    "Found expected number of tags in the tags selector"
  );

  Array.prototype.forEach.call(children, function(aChild) {
    let tag = aChild.querySelector("label").getAttribute("value");
    ok(true, "Found tag '" + tag + "' in the selector");
    ok(aAvailableTags.includes(tag), "Found expected tag");
    let checked = aChild.getAttribute("checked") == "true";
    is(checked, aCheckedTags.includes(tag), "Tag is correctly marked");
  });
}

async function promiseTagSelectorUpdated(task) {
  let tagsSelector = document.getElementById("editBMPanel_tagsSelector");

  let promise = BrowserTestUtils.waitForEvent(
    tagsSelector,
    "BookmarkTagsSelectorUpdated"
  );

  await task();
  return promise;
}

add_task(async function() {
  const TEST_URI = Services.io.newURI("http://www.test.me/");
  const TEST_URI2 = Services.io.newURI("http://www.test.again.me/");
  const TEST_TAG = "test-tag";

  ok(gEditItemOverlay, "Sanity check: gEditItemOverlay is in context");

  // Open the tags selector.
  StarUI._createPanelIfNeeded();
  document.getElementById("editBMPanel_tagsSelectorRow").collapsed = false;

  // Add a bookmark.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: TEST_URI.spec,
    title: "test.me",
  });

  // Init panel.
  let node = await PlacesUIUtils.promiseNodeLikeFromFetchInfo(bm);
  gEditItemOverlay.initPanel({ node });

  // Add a tag.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.tagURI(TEST_URI, [TEST_TAG])
  );

  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI)[0],
    TEST_TAG,
    "Correctly added tag to a single bookmark"
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    TEST_TAG,
    "Editing a single bookmark shows the added tag."
  );
  await checkTagsSelector([TEST_TAG], [TEST_TAG]);

  // Remove tag.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.untagURI(TEST_URI, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI)[0],
    undefined,
    "The tag has been removed"
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    "",
    "Editing a single bookmark should not show any tag"
  );
  await checkTagsSelector([], []);

  // Add a second bookmark.
  let bm2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    title: "test.again.me",
    url: TEST_URI2.spec,
  });

  // Init panel with multiple uris.
  gEditItemOverlay.initPanel({ uris: [TEST_URI, TEST_URI2] });

  // Add a tag to the first uri.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.tagURI(TEST_URI, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI)[0],
    TEST_TAG,
    "Correctly added a tag to the first bookmark."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    "",
    "Editing multiple bookmarks without matching tags should not show any tag."
  );
  await checkTagsSelector([TEST_TAG], []);

  // Add a tag to the second uri.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.tagURI(TEST_URI2, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI2)[0],
    TEST_TAG,
    "Correctly added a tag to the second bookmark."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    TEST_TAG,
    "Editing multiple bookmarks should show matching tags."
  );
  await checkTagsSelector([TEST_TAG], [TEST_TAG]);

  // Remove tag from the first bookmark.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.untagURI(TEST_URI, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI)[0],
    undefined,
    "Correctly removed tag from the first bookmark."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    "",
    "Editing multiple bookmarks without matching tags should not show any tag."
  );
  await checkTagsSelector([TEST_TAG], []);

  // Remove tag from the second bookmark.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.untagURI(TEST_URI2, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI2)[0],
    undefined,
    "Correctly removed tag from the second bookmark."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    "",
    "Editing multiple bookmarks without matching tags should not show any tag."
  );
  await checkTagsSelector([], []);

  // Init panel with a nsIURI entry.
  gEditItemOverlay.initPanel({ uris: [TEST_URI] });

  // Add a tag.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.tagURI(TEST_URI, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI)[0],
    TEST_TAG,
    "Correctly added tag to the first entry."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    TEST_TAG,
    "Editing a single nsIURI entry shows the added tag."
  );
  await checkTagsSelector([TEST_TAG], [TEST_TAG]);

  // Remove tag.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.untagURI(TEST_URI, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI)[0],
    undefined,
    "Correctly removed tag from the nsIURI entry."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    "",
    "Editing a single nsIURI entry should not show any tag."
  );
  await checkTagsSelector([], []);

  // Init panel with multiple nsIURI entries.
  gEditItemOverlay.initPanel({ uris: [TEST_URI, TEST_URI2] });

  // Add a tag to the first entry.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.tagURI(TEST_URI, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI)[0],
    TEST_TAG,
    "Tag correctly added."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    "",
    "Editing multiple nsIURIs without matching tags should not show any tag."
  );
  await checkTagsSelector([TEST_TAG], []);

  // Add a tag to the second entry.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.tagURI(TEST_URI2, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI2)[0],
    TEST_TAG,
    "Tag correctly added."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    TEST_TAG,
    "Editing multiple nsIURIs should show matching tags."
  );
  await checkTagsSelector([TEST_TAG], [TEST_TAG]);

  // Remove tag from the first entry.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.untagURI(TEST_URI, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI)[0],
    undefined,
    "Correctly removed tag from the first entry."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    "",
    "Editing multiple nsIURIs without matching tags should not show any tag."
  );
  await checkTagsSelector([TEST_TAG], []);

  // Remove tag from the second entry.
  await promiseTagSelectorUpdated(() =>
    PlacesUtils.tagging.untagURI(TEST_URI2, [TEST_TAG])
  );
  is(
    PlacesUtils.tagging.getTagsForURI(TEST_URI2)[0],
    undefined,
    "Correctly removed tag from the second entry."
  );
  Assert.equal(
    document.getElementById("editBMPanel_tagsField").value,
    "",
    "Editing multiple nsIURIs without matching tags should not show any tag."
  );
  await checkTagsSelector([], []);

  // Cleanup.
  await PlacesUtils.bookmarks.remove(bm.guid);
  await PlacesUtils.bookmarks.remove(bm2.guid);
});
