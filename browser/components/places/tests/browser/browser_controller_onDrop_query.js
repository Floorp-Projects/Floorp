/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "about:buildconfig";

add_setup(async function () {
  // Clean before and after so we don't have anything in the folders.
  await PlacesUtils.bookmarks.eraseEverything();

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

// Simulating actual drag and drop is hard for a xul tree as we can't get the
// required source elements, so we have to do a lot more work by hand.
async function simulateDrop(
  selectTargets,
  sourceBm,
  dropEffect,
  targetGuid,
  isVirtualRoot = false
) {
  await withSidebarTree("bookmarks", async function (tree) {
    for (let target of selectTargets) {
      tree.selectItems([target]);
      if (tree.selectedNode instanceof Ci.nsINavHistoryContainerResultNode) {
        tree.selectedNode.containerOpen = true;
      }
    }

    let dataTransfer = {
      _data: [],
      dropEffect,
      mozCursor: "auto",
      mozItemCount: 1,
      types: [PlacesUtils.TYPE_X_MOZ_PLACE],
      mozTypesAt(i) {
        return [this._data[0].type];
      },
      mozGetDataAt(i) {
        return this._data[0].data;
      },
      mozSetDataAt(type, data, index) {
        this._data.push({
          type,
          data,
          index,
        });
      },
    };

    let event = {
      dataTransfer,
      preventDefault() {},
      stopPropagation() {},
    };

    tree._controller.setDataTransfer(event);

    Assert.equal(
      dataTransfer.mozTypesAt(0),
      PlacesUtils.TYPE_X_MOZ_PLACE,
      "Should have x-moz-place as the first data type."
    );

    let dataObject = JSON.parse(dataTransfer.mozGetDataAt(0));

    let guid = isVirtualRoot ? dataObject.concreteGuid : dataObject.itemGuid;

    Assert.equal(guid, sourceBm.guid, "Should have the correct guid.");
    Assert.equal(
      dataObject.title,
      PlacesUtils.bookmarks.getLocalizedTitle(sourceBm),
      "Should have the correct title."
    );

    Assert.equal(dataTransfer.dropEffect, dropEffect);

    let ip = new PlacesInsertionPoint({
      parentId: await PlacesTestUtils.promiseItemId(targetGuid),
      parentGuid: targetGuid,
      index: 0,
      orientation: Ci.nsITreeView.DROP_ON,
    });

    await PlacesControllerDragHelper.onDrop(ip, dataTransfer);
  });
}

add_task(async function test_move_out_of_query() {
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Fake",
    url: TEST_URL,
  });

  let queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;

  let queries = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        title: "Query",
        url: `place:queryType=${queryType}&terms=Fake`,
      },
    ],
  });
  await simulateDrop(
    [queries[0].guid, bookmark.guid],
    bookmark,
    "move",
    PlacesUtils.bookmarks.toolbarGuid
  );

  let newBm = await PlacesUtils.bookmarks.fetch(bookmark.guid);

  Assert.equal(
    newBm.parentGuid,
    PlacesUtils.bookmarks.toolbarGuid,
    "should have moved the bookmark to a new folder."
  );

  let oldLocationBm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });

  Assert.ok(!oldLocationBm, "Should not have a bookmark at the old location.");
});
