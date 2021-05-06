/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var bookmarks;
var bookmarkIds;
var library;

add_task(async function setup() {
  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
    await promiseLibraryClosed(library);
  });

  bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "bm1",
        url: "http://example1.com",
      },
      {
        title: "bm2",
        url: "http://example2.com",
      },
      {
        title: "bm3",
        url: "http://example3.com",
      },
    ],
  });

  bookmarkIds = await PlacesUtils.promiseManyItemIds([
    bookmarks[0].guid,
    bookmarks[1].guid,
    bookmarks[2].guid,
  ]);

  library = await promiseLibrary("UnfiledBookmarks");
});

async function run_drag_test(startBookmarkIndex, insertionIndex) {
  let dragBookmark = bookmarks[startBookmarkIndex];

  library.ContentTree.view.selectItems([dragBookmark.guid]);

  let dataTransfer = {
    _data: [],
    dropEffect: "move",
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

  library.ContentTree.view.controller.setDataTransfer(event);

  Assert.equal(
    dataTransfer.mozTypesAt(0),
    PlacesUtils.TYPE_X_MOZ_PLACE,
    "Should have x-moz-place as the first data type."
  );

  let dataObject = JSON.parse(dataTransfer.mozGetDataAt(0));

  Assert.equal(
    dataObject.itemGuid,
    dragBookmark.guid,
    "Should have the correct guid."
  );
  Assert.equal(
    dataObject.title,
    dragBookmark.title,
    "Should have the correct title."
  );

  Assert.equal(dataTransfer.dropEffect, "move");

  let ip = new PlacesInsertionPoint({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: insertionIndex,
    orientation: Ci.nsITreeView.DROP_ON,
  });

  await PlacesControllerDragHelper.onDrop(ip, dataTransfer);
}

add_task(async function test_simple_move_down() {
  let moveNotification = PlacesTestUtils.waitForNotification(
    "bookmark-moved",
    events =>
      events.some(
        e => e.guid === bookmarks[0].guid && e.oldIndex == 0 && e.index == 1
      ),
    "places"
  );

  await run_drag_test(0, 2);

  await moveNotification;
});

add_task(async function test_simple_move_up() {
  await run_drag_test(2, 0);
});
