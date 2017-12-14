/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

const sandbox = sinon.sandbox.create();

var bookmarks;
var bookmarkIds;

add_task(async function setup() {
  registerCleanupFunction(async function() {
    sandbox.restore();
    delete window.sinon;
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesTestUtils.clearHistory();
  });

  sandbox.stub(PlacesUIUtils, "getTransactionForData");
  sandbox.stub(PlacesTransactions, "batch");

  bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "bm1",
      url: "http://example1.com"
    }, {
      title: "bm2",
      url: "http://example2.com"
    }, {
      title: "bm3",
      url: "http://example3.com"
    }, {
      title: "folder1",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        title: "bm4",
        url: "http://example1.com"
      }, {
        title: "bm5",
        url: "http://example2.com"
      }, {
        title: "bm6",
        url: "http://example3.com"
      }]
    }]
  });

  bookmarkIds = await PlacesUtils.promiseManyItemIds([
    bookmarks[0].guid,
    bookmarks[1].guid,
    bookmarks[2].guid,
  ]);
});

async function run_drag_test(startBookmarkIndex, insertionIndex, newParentGuid,
                             expectedInsertionIndex, expectTransactionCreated = true) {
  if (!PlacesUIUtils.useAsyncTransactions) {
    Assert.ok(true, "Skipping test as async transactions are turned off");
    return;
  }

  if (!newParentGuid) {
    newParentGuid = PlacesUtils.bookmarks.unfiledGuid;
  }

  // Reset the stubs so that previous test runs don't count against us.
  PlacesUIUtils.getTransactionForData.reset();
  PlacesTransactions.batch.reset();

  let dragBookmark = bookmarks[startBookmarkIndex];

  await withSidebarTree("bookmarks", async (tree) => {
    tree.selectItems([PlacesUtils.bookmarks.unfiledGuid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;

    // Simulating a drag-drop with a tree view turns out to be really difficult
    // as you can't get a node for the source/target. Hence, we fake the
    // insertion point and drag data and call the function direct.
    let ip = new InsertionPoint({
      parentId: await PlacesUtils.promiseItemId(PlacesUtils.bookmarks.unfiledGuid),
      parentGuid: newParentGuid,
      index: insertionIndex,
      orientation: Ci.nsITreeView.DROP_ON
    });

    let bookmarkWithId = JSON.stringify(Object.assign({
      id: bookmarkIds.get(dragBookmark.guid),
      itemGuid: dragBookmark.guid,
      parent: PlacesUtils.unfiledBookmarksFolderId,
    }, dragBookmark));

    let dt = {
      dropEffect: "move",
      mozCursor: "auto",
      mozItemCount: 1,
      types: [ PlacesUtils.TYPE_X_MOZ_PLACE ],
      mozTypesAt(i) {
        return this.types;
      },
      mozGetDataAt(i) {
        return bookmarkWithId;
      }
    };

    await PlacesControllerDragHelper.onDrop(ip, dt);

    if (!expectTransactionCreated) {
      Assert.ok(PlacesUIUtils.getTransactionForData.notCalled,
        "Should not have created transaction data");
      return;
    }

    Assert.ok(PlacesUIUtils.getTransactionForData.calledOnce,
      "Should have called getTransactionForData at least once.");

    let args = PlacesUIUtils.getTransactionForData.args[0];

    Assert.deepEqual(args[0], JSON.parse(bookmarkWithId),
      "Should have called getTransactionForData with the correct unwrapped bookmark");
    Assert.equal(args[1], newParentGuid,
      "Should have called getTransactionForData with the correct parent guid");
    Assert.equal(args[2], expectedInsertionIndex,
      "Should have called getTransactionForData with the correct index");
    Assert.equal(args[3], false,
      "Should have called getTransactionForData with a move");
  });
}

add_task(async function test_simple_move_down() {
  // When we move items down the list, we'll get a drag index that is one higher
  // than where we actually want to insert to - as the item is being moved up,
  // everything shifts down one. Hence the index to pass to the transaction should
  // be one less than the supplied index.
  await run_drag_test(0, 2, null, 1);
});

add_task(async function test_simple_move_up() {
  // When we move items up the list, we want the matching index to be passed to
  // the transaction as there's no changes below the item in the list.
  await run_drag_test(2, 0, null, 0);
});

add_task(async function test_simple_move_to_same_index() {
  // If we move to the same index, then we don't expect any transactions to be
  // created.
  await run_drag_test(1, 1, null, 1, false);
});

add_task(async function test_simple_move_different_folder_append() {
  // When we move items to a different folder, the insertion index will be -1
  // and shouldn't change.
  await run_drag_test(0, -1, bookmarks[3].guid, -1);
  await run_drag_test(2, -1, bookmarks[3].guid, -1);
});

add_task(async function test_move_different_folder_insert_at() {
  // When we move items to a different folder, the insertion index will be -1
  // and shouldn't change.
  await run_drag_test(0, 0, bookmarks[3].guid, 0);
  await run_drag_test(2, 2, bookmarks[3].guid, 2);
});
