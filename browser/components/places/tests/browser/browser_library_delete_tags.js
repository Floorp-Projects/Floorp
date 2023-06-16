/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test enabled commands in the left pane folder of the Library.
 */

registerCleanupFunction(async function () {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function test_tags() {
  const TEST_URI = Services.io.newURI("http://example.com/");

  await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: TEST_URI,
    title: "example page",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  PlacesUtils.tagging.tagURI(TEST_URI, ["test"]);

  let library = await promiseLibrary();

  // Select and open the left pane "Bookmarks Toolbar" folder.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneBuiltIn("Tags");
  let tagsNode = PO._places.selectedNode;
  Assert.notEqual(tagsNode, null, "Should have a valid selection");
  let tagsTitle = PlacesUtils.getString("TagsFolderTitle");
  Assert.equal(tagsNode.title, tagsTitle, "Should have selected the Tags node");

  // Now select the tag.
  PlacesUtils.asContainer(tagsNode).containerOpen = true;
  let tag = tagsNode.getChild(0);
  PO._places.selectNode(tag);
  Assert.equal(
    PO._places.selectedNode.title,
    "test",
    "Should have selected the created tag"
  );

  PO._places.controller.doCommand("cmd_delete");

  await PlacesTestUtils.promiseAsyncUpdates();

  let tags = PlacesUtils.tagging.getTagsForURI(TEST_URI);

  Assert.equal(tags.length, 0, "There should be no tags for the URI");

  tagsNode.containerOpen = false;

  library.close();
});
