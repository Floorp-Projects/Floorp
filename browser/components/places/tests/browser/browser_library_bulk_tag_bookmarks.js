/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that a tag can be added to multiple bookmarks at once from the library.
 */
"use strict";

const TEST_URLS = ["about:buildconfig", "about:robots"];

add_task(async function test_bulk_tag_from_library() {
  // Create multiple bookmarks.
  for (const url of TEST_URLS) {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url,
    });
  }

  // Open library panel.
  const library = await promiseLibrary("UnfiledBookmarks");
  const cleanupFn = async () => {
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.eraseEverything();
  };
  registerCleanupFunction(cleanupFn);

  // Add a tag to multiple bookmarks.
  library.ContentTree.view.selectAll();
  const promiseAllTagsChanged = TEST_URLS.map(url =>
    PlacesTestUtils.waitForNotification("bookmark-tags-changed", events =>
      events.some(evt => evt.url === url)
    )
  );
  const tag = "some, tag";
  const tagWithDuplicates = `${tag}, tag`;
  fillBookmarkTextField("editBMPanel_tagsField", tagWithDuplicates, library);
  await Promise.all(promiseAllTagsChanged);
  await TestUtils.waitForCondition(
    () =>
      library.document.getElementById("editBMPanel_tagsField").value === tag,
    "Input field matches the new tags and duplicates are removed."
  );

  // Verify that the bookmarks were tagged successfully.
  for (const url of TEST_URLS) {
    Assert.deepEqual(
      PlacesUtils.tagging.getTagsForURI(Services.io.newURI(url)),
      ["some", "tag"],
      url + " should have the correct tags."
    );
  }
  await cleanupFn();
});

add_task(async function test_bulk_tag_tags_selector() {
  // Create multiple bookmarks with a common tag.
  for (const [i, url] of TEST_URLS.entries()) {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url,
    });
    PlacesUtils.tagging.tagURI(Services.io.newURI(url), [
      "common",
      `unique_${i}`,
    ]);
  }

  // Open library panel.
  const library = await promiseLibrary("UnfiledBookmarks");
  const cleanupFn = async () => {
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.eraseEverything();
  };
  registerCleanupFunction(cleanupFn);

  // Open tags selector.
  library.document.getElementById("editBMPanel_tagsSelectorRow").hidden = false;

  // Select all bookmarks.
  const tagsSelector = library.document.getElementById(
    "editBMPanel_tagsSelector"
  );
  library.ContentTree.view.selectAll();

  // Verify that the input field only shows the common tag.
  await TestUtils.waitForCondition(
    () =>
      library.document.getElementById("editBMPanel_tagsField").value ===
      "common",
    "Input field only shows the common tag."
  );

  // Verify that the tags selector shows all tags, and only the common one is
  // checked.
  async function checkTagsSelector(aAvailableTags, aCheckedTags) {
    let tags = await PlacesUtils.bookmarks.fetchTags();
    is(tags.length, aAvailableTags.length, "Check tags list");
    let children = tagsSelector.children;
    is(
      children.length,
      aAvailableTags.length,
      "Found expected number of tags in the tags selector"
    );

    Array.prototype.forEach.call(children, function (aChild) {
      let tag = aChild.querySelector("label").getAttribute("value");
      ok(true, "Found tag '" + tag + "' in the selector");
      ok(aAvailableTags.includes(tag), "Found expected tag");
      let checked = aChild.getAttribute("checked") == "true";
      is(checked, aCheckedTags.includes(tag), "Tag is correctly marked");
    });
  }

  async function promiseTagSelectorUpdated(task) {
    let promise = BrowserTestUtils.waitForEvent(
      tagsSelector,
      "BookmarkTagsSelectorUpdated"
    );

    await task();
    return promise;
  }

  info("Check the initial common tag.");
  await checkTagsSelector(["common", "unique_0", "unique_1"], ["common"]);

  // Verify that the common tag can be edited.
  await promiseTagSelectorUpdated(() => {
    info("Edit the common tag.");
    fillBookmarkTextField("editBMPanel_tagsField", "common_updated", library);
  });
  await checkTagsSelector(
    ["common_updated", "unique_0", "unique_1"],
    ["common_updated"]
  );

  // Verify that the common tag can be removed.
  await promiseTagSelectorUpdated(() => {
    info("Remove the commmon tag.");
    fillBookmarkTextField("editBMPanel_tagsField", "", library);
  });
  await checkTagsSelector(["unique_0", "unique_1"], []);

  await cleanupFn();
});
