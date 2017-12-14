/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

const sandbox = sinon.sandbox.create();
const TAG_NAME = "testTag";

var bookmarks;
var bookmarkId;

add_task(async function setup() {
  registerCleanupFunction(async function() {
    sandbox.restore();
    delete window.sinon;
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesTestUtils.clearHistory();
  });

  sandbox.stub(PlacesTransactions, "batch");
  sandbox.stub(PlacesTransactions, "Tag");

  bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "bm1",
      url: "http://example1.com"
    }, {
      title: "bm2",
      url: "http://example2.com",
      tags: [TAG_NAME]
    }]
  });
  bookmarkId = await PlacesUtils.promiseItemId(bookmarks[0].guid);
});

async function run_drag_test(startBookmarkIndex, newParentGuid) {
  if (!PlacesUIUtils.useAsyncTransactions) {
    Assert.ok(true, "Skipping test as async transactions are turned off");
    return;
  }

  if (!newParentGuid) {
    newParentGuid = PlacesUtils.bookmarks.unfiledGuid;
  }

  // Reset the stubs so that previous test runs don't count against us.
  PlacesTransactions.Tag.reset();
  PlacesTransactions.batch.reset();

  let dragBookmark = bookmarks[startBookmarkIndex];

  await withSidebarTree("bookmarks", async (tree) => {
    tree.selectItems([PlacesUtils.bookmarks.unfiledGuid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;

    // Simulating a drag-drop with a tree view turns out to be really difficult
    // as you can't get a node for the source/target. Hence, we fake the
    // insertion point and drag data and call the function direct.
    let ip = new InsertionPoint({
      isTag: true,
      tagName: TAG_NAME,
      orientation: Ci.nsITreeView.DROP_ON
    });

    let bookmarkWithId = JSON.stringify(Object.assign({
      id: bookmarkId,
      itemGuid: dragBookmark.guid,
      parent: PlacesUtils.unfiledBookmarksFolderId,
      uri: dragBookmark.url
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

    Assert.ok(PlacesTransactions.Tag.calledOnce,
      "Should have called getTransactionForData at least once.");

    let arg = PlacesTransactions.Tag.args[0][0];

    Assert.equal(arg.urls.length, 1,
      "Should have called PlacesTransactions.Tag with an array of one url");
    Assert.equal(arg.urls[0], dragBookmark.url,
      "Should have called PlacesTransactions.Tag with the correct url");
    Assert.equal(arg.tag, TAG_NAME,
      "Should have called PlacesTransactions.Tag with the correct tag name");
  });
}

add_task(async function test_simple_drop_and_tag() {
  // When we move items down the list, we'll get a drag index that is one higher
  // than where we actually want to insert to - as the item is being moved up,
  // everything shifts down one. Hence the index to pass to the transaction should
  // be one less than the supplied index.
  await run_drag_test(0, PlacesUtils.bookmarks.tagGuid);
});
