/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test whether tags are shown in library window.
 */

registerCleanupFunction(async function () {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

const TEST_URI = Services.io.newURI("https://example.com/");
const TEST_TAG = "testtag";

add_task(async function base() {
  const { guid } = await PlacesUtils.bookmarks.insert({
    url: TEST_URI,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  await PlacesTestUtils.addVisits(TEST_URI);
  PlacesUtils.tagging.tagURI(TEST_URI, [TEST_TAG]);

  const library = await promiseLibrary();
  registerCleanupFunction(() => {
    library.close();
  });

  const {
    document: libraryDocument,
    PlacesOrganizer: placesOrganizer,
    ContentTree: contentTree,
  } = library;

  info("Test in Bookmarks");
  placesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
  contentTree.view.selectItems([guid]);
  await assertTagsVisibility(libraryDocument, true);

  info("Test in Tags");
  placesOrganizer.selectLeftPaneBuiltIn("Tags");
  const tagsContainer = PlacesUtils.asContainer(
    placesOrganizer._places.selectedNode
  );
  tagsContainer.containerOpen = true;
  const targetNode = tagsContainer.getChild(0);
  placesOrganizer._places.selectNode(targetNode);
  contentTree.view.selectItems([guid]);
  await assertTagsVisibility(libraryDocument, true);

  info("Test in History");
  placesOrganizer.selectLeftPaneBuiltIn("History");
  const historyNode = PlacesUtils.asContainer(
    placesOrganizer._places.selectedNode
  );
  historyNode.containerOpen = true;
  const todayNode = historyNode.getChild(0);
  placesOrganizer._places.selectNode(todayNode);
  contentTree.view.selectItems([guid]);
  await assertTagsVisibility(libraryDocument, false);
});

async function assertTagsVisibility(libraryDocument, expectedVisible) {
  const locationInput = libraryDocument.getElementById(
    "editBMPanel_locationField"
  );
  await BrowserTestUtils.waitForCondition(
    () => locationInput.value === TEST_URI.spec,
    "Wait until the panel ready"
  );

  // Check the editor area.
  const tagInput = libraryDocument.getElementById("editBMPanel_tagsField");
  Assert.equal(BrowserTestUtils.is_visible(tagInput), expectedVisible);
  if (expectedVisible) {
    Assert.equal(tagInput.value, TEST_TAG);
  }

  // Check the cell.
  const tree = libraryDocument.getElementById("placeContent");
  const expectedCellText = expectedVisible ? TEST_TAG : null;
  Assert.equal(
    tree.view.getCellText(0, tree.columns.placesContentTags),
    expectedCellText
  );
}
