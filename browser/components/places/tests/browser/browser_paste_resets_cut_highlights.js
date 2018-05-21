/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "http://example.com/";
const TEST_URL1 = "https://example.com/otherbrowser/";

var PlacesOrganizer;
var ContentTree;

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();
  let organizer = await promiseLibrary();

  registerCleanupFunction(async function() {
    await promiseLibraryClosed(organizer);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  PlacesOrganizer = organizer.PlacesOrganizer;
  ContentTree = organizer.ContentTree;
});

add_task(async function paste() {
  info("Selecting BookmarksToolbar in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");

  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [{
      url: TEST_URL,
      title: "0",
    }, {
      url: TEST_URL1,
      title: "1",
    }]
  });

  Assert.equal(ContentTree.view.view.rowCount, 2,
    "Should have the right amount of items in the view");

  ContentTree.view.selectItems([bookmarks[0].guid]);

  await promiseClipboard(() => {
    info("Cutting selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  assertItemsHighlighted(1);

  ContentTree.view.selectItems([bookmarks[1].guid]);

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  assertItemsHighlighted(0);

  // And now repeat the other way around to make sure.

  await promiseClipboard(() => {
    info("Cutting selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  assertItemsHighlighted(1);

  ContentTree.view.selectItems([bookmarks[0].guid]);

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  assertItemsHighlighted(0);
});

function assertItemsHighlighted(expectedItems) {
  let column = ContentTree.view.view._tree.columns[0];
  // Check the properties of the cells to make sure nothing has a cut highlight.
  let highlighedItems = 0;
  for (let i = 0; i < ContentTree.view.view.rowCount; i++) {
    if (ContentTree.view.view.getCellProperties(i, column).includes("cutting")) {
      highlighedItems++;
    }
  }

  Assert.equal(highlighedItems, expectedItems,
    "Should have the correct amount of items highlighed");
}
