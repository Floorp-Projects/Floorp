/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "http://example.com/";
const TEST_URL1 = "https://example.com/otherbrowser/";

var PlacesOrganizer;
var ContentTree;

add_setup(async function () {
  await PlacesUtils.bookmarks.eraseEverything();
  let organizer = await promiseLibrary();

  registerCleanupFunction(async function () {
    await promiseLibraryClosed(organizer);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  PlacesOrganizer = organizer.PlacesOrganizer;
  ContentTree = organizer.ContentTree;

  // Show date added column.
  await showLibraryColumn(organizer, "placesContentDateAdded");
});

add_task(async function paste() {
  info("Selecting BookmarksToolbar in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");

  let dateAdded = new Date();
  dateAdded.setHours(10);
  dateAdded.setMinutes(10);
  dateAdded.setSeconds(0);

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URL,
    title: "0",
    dateAdded,
  });

  ContentTree.view.selectItems([bookmark.guid]);

  await promiseClipboard(() => {
    info("Cutting selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  let tree = await PlacesUtils.promiseBookmarksTree(
    PlacesUtils.bookmarks.unfiledGuid
  );

  Assert.equal(
    tree.children.length,
    1,
    "Should be one bookmark in the unfiled folder."
  );
  Assert.equal(tree.children[0].title, "0", "Should have the correct title");
  Assert.equal(tree.children[0].uri, TEST_URL, "Should have the correct URL");
  Assert.equal(
    tree.children[0].dateAdded,
    PlacesUtils.toPRTime(dateAdded),
    "Should have the correct date"
  );

  Assert.ok(
    ContentTree.view.view
      .getCellText(0, ContentTree.view.columns.placesContentDateAdded)
      .startsWith(`${dateAdded.getHours()}:${dateAdded.getMinutes()}`),
    "Should reflect the data added"
  );

  await PlacesUtils.bookmarks.remove(tree.children[0].guid);
});

add_task(async function paste_check_indexes() {
  info("Selecting BookmarksToolbar in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");

  let copyChildren = [];
  let targetChildren = [];
  for (let i = 0; i < 10; i++) {
    copyChildren.push({
      url: `${TEST_URL}${i}`,
      title: `Copy ${i}`,
    });
    targetChildren.push({
      url: `${TEST_URL1}${i}`,
      title: `Target ${i}`,
    });
  }

  let copyBookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: copyChildren,
  });

  let targetBookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: targetChildren,
  });

  ContentTree.view.selectItems([
    copyBookmarks[0].guid,
    copyBookmarks[3].guid,
    copyBookmarks[6].guid,
    copyBookmarks[9].guid,
  ]);

  await promiseClipboard(() => {
    info("Cutting multiple selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  ContentTree.view.selectItems([targetBookmarks[4].guid]);

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  let tree = await PlacesUtils.promiseBookmarksTree(
    PlacesUtils.bookmarks.unfiledGuid
  );

  const expectedBookmarkOrder = [
    targetBookmarks[0].guid,
    targetBookmarks[1].guid,
    targetBookmarks[2].guid,
    targetBookmarks[3].guid,
    copyBookmarks[0].guid,
    copyBookmarks[3].guid,
    copyBookmarks[6].guid,
    copyBookmarks[9].guid,
    targetBookmarks[4].guid,
    targetBookmarks[5].guid,
    targetBookmarks[6].guid,
    targetBookmarks[7].guid,
    targetBookmarks[8].guid,
    targetBookmarks[9].guid,
  ];

  Assert.equal(
    tree.children.length,
    expectedBookmarkOrder.length,
    "Should be the expected amount of bookmarks in the unfiled folder."
  );

  for (let i = 0; i < expectedBookmarkOrder.length; ++i) {
    Assert.equal(
      tree.children[i].guid,
      expectedBookmarkOrder[i],
      `Should be the expected item at index ${i}`
    );
  }

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function paste_check_indexes_same_folder() {
  info("Selecting BookmarksToolbar in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");

  let copyChildren = [];
  for (let i = 0; i < 10; i++) {
    copyChildren.push({
      url: `${TEST_URL}${i}`,
      title: `Copy ${i}`,
    });
  }

  let copyBookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: copyChildren,
  });

  ContentTree.view.selectItems([
    copyBookmarks[0].guid,
    copyBookmarks[3].guid,
    copyBookmarks[6].guid,
    copyBookmarks[9].guid,
  ]);

  await promiseClipboard(() => {
    info("Cutting multiple selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  ContentTree.view.selectItems([copyBookmarks[4].guid]);

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  let tree = await PlacesUtils.promiseBookmarksTree(
    PlacesUtils.bookmarks.toolbarGuid
  );

  // Although we've inserted at index 4, we've taken out two items below it, so
  // we effectively insert after the third item.
  const expectedBookmarkOrder = [
    copyBookmarks[1].guid,
    copyBookmarks[2].guid,
    copyBookmarks[0].guid,
    copyBookmarks[3].guid,
    copyBookmarks[6].guid,
    copyBookmarks[9].guid,
    copyBookmarks[4].guid,
    copyBookmarks[5].guid,
    copyBookmarks[7].guid,
    copyBookmarks[8].guid,
  ];

  Assert.equal(
    tree.children.length,
    expectedBookmarkOrder.length,
    "Should be the expected amount of bookmarks in the unfiled folder."
  );

  for (let i = 0; i < expectedBookmarkOrder.length; ++i) {
    Assert.equal(
      tree.children[i].guid,
      expectedBookmarkOrder[i],
      `Should be the expected item at index ${i}`
    );
  }

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function paste_from_different_instance() {
  let xferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  xferable.init(null);

  // Fake data on the clipboard to pretend this is from a different instance
  // of Firefox.
  let data = {
    title: "test",
    id: 32,
    instanceId: "FAKEFAKEFAKE",
    itemGuid: "ZBf_TYkrYGvW",
    parent: 452,
    dateAdded: 1464866275853000,
    lastModified: 1507638113352000,
    type: "text/x-moz-place",
    uri: TEST_URL1,
  };
  data = JSON.stringify(data);

  xferable.addDataFlavor(PlacesUtils.TYPE_X_MOZ_PLACE);
  xferable.setTransferData(
    PlacesUtils.TYPE_X_MOZ_PLACE,
    PlacesUtils.toISupportsString(data)
  );

  Services.clipboard.setData(xferable, null, Ci.nsIClipboard.kGlobalClipboard);

  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  let tree = await PlacesUtils.promiseBookmarksTree(
    PlacesUtils.bookmarks.unfiledGuid
  );

  Assert.equal(
    tree.children.length,
    1,
    "Should be one bookmark in the unfiled folder."
  );
  Assert.equal(tree.children[0].title, "test", "Should have the correct title");
  Assert.equal(tree.children[0].uri, TEST_URL1, "Should have the correct URL");

  await PlacesUtils.bookmarks.remove(tree.children[0].guid);
});

add_task(async function paste_separator_from_different_instance() {
  let xferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  xferable.init(null);

  // Fake data on the clipboard to pretend this is from a different instance
  // of Firefox.
  let data = {
    title: "test",
    id: 32,
    instanceId: "FAKEFAKEFAKE",
    itemGuid: "ZBf_TYkrYGvW",
    parent: 452,
    dateAdded: 1464866275853000,
    lastModified: 1507638113352000,
    type: PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
  };
  data = JSON.stringify(data);

  xferable.addDataFlavor(PlacesUtils.TYPE_X_MOZ_PLACE);
  xferable.setTransferData(
    PlacesUtils.TYPE_X_MOZ_PLACE,
    PlacesUtils.toISupportsString(data)
  );

  Services.clipboard.setData(xferable, null, Ci.nsIClipboard.kGlobalClipboard);

  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  let tree = await PlacesUtils.promiseBookmarksTree(
    PlacesUtils.bookmarks.unfiledGuid
  );

  Assert.equal(
    tree.children.length,
    1,
    "Should be one bookmark in the unfiled folder."
  );
  Assert.equal(
    tree.children[0].type,
    PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
    "Should have the correct type"
  );

  await PlacesUtils.bookmarks.remove(tree.children[0].guid);
});

add_task(async function paste_copy_check_indexes() {
  info("Selecting BookmarksToolbar in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");

  let copyChildren = [];
  let targetChildren = [];
  for (let i = 0; i < 10; i++) {
    copyChildren.push({
      url: `${TEST_URL}${i}`,
      title: `Copy ${i}`,
    });
    targetChildren.push({
      url: `${TEST_URL1}${i}`,
      title: `Target ${i}`,
    });
  }

  let copyBookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: copyChildren,
  });

  let targetBookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: targetChildren,
  });

  ContentTree.view.selectItems([
    copyBookmarks[0].guid,
    copyBookmarks[3].guid,
    copyBookmarks[6].guid,
    copyBookmarks[9].guid,
  ]);

  await promiseClipboard(() => {
    info("Cutting multiple selection");
    ContentTree.view.controller.copy();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  ContentTree.view.selectItems([targetBookmarks[4].guid]);

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  let tree = await PlacesUtils.promiseBookmarksTree(
    PlacesUtils.bookmarks.unfiledGuid
  );

  const expectedBookmarkOrder = [
    targetBookmarks[0].guid,
    targetBookmarks[1].guid,
    targetBookmarks[2].guid,
    targetBookmarks[3].guid,
    0,
    3,
    6,
    9,
    targetBookmarks[4].guid,
    targetBookmarks[5].guid,
    targetBookmarks[6].guid,
    targetBookmarks[7].guid,
    targetBookmarks[8].guid,
    targetBookmarks[9].guid,
  ];

  Assert.equal(
    tree.children.length,
    expectedBookmarkOrder.length,
    "Should be the expected amount of bookmarks in the unfiled folder."
  );

  for (let i = 0; i < expectedBookmarkOrder.length; ++i) {
    if (i > 3 && i <= 7) {
      // Items 4 - 7 are copies of the original, so we need to compare data, rather
      // than their guids.
      Assert.equal(
        tree.children[i].title,
        copyChildren[expectedBookmarkOrder[i]].title,
        `Should have the correct bookmark title at index ${i}`
      );
      Assert.equal(
        tree.children[i].uri,
        copyChildren[expectedBookmarkOrder[i]].url,
        `Should have the correct bookmark URL at index ${i}`
      );
    } else {
      Assert.equal(
        tree.children[i].guid,
        expectedBookmarkOrder[i],
        `Should be the expected item at index ${i}`
      );
    }
  }

  await PlacesUtils.bookmarks.eraseEverything();
});
