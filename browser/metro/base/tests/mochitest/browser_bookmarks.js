// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let gStartView = BookmarksStartView._view;

function test() {
  runTests();
}

function setup() {
  PanelUI.hide();
  BookmarksTestHelper.setup();

  yield hideContextUI();

  if (StartUI.isStartPageVisible)
    return;

  yield addTab("about:start");

  yield waitForCondition(() => StartUI.isStartPageVisible);
}

function tearDown() {
  PanelUI.hide();
  BookmarksTestHelper.restore();
}

var BookmarksTestHelper = {
  _originalNavHistoryService: null,
  MockNavHistoryService: {
    getNewQueryOptions: function () {
      return {};
    },
    getNewQuery: function () {
      return {
        setFolders: function(){}
      };
    },
    executeQuery: function () {
      return {
        root: {
          get childCount() {
            return Object.keys(BookmarksTestHelper._nodes).length;
          },

          getChild: function (aIndex) BookmarksTestHelper._nodes[Object.keys(BookmarksTestHelper._nodes)[aIndex]]
        }
      }
    }
  },

  _originalBookmarkService: null,
  MockBookmarkService: {
    getItemIndex: function (aIndex) aIndex,
    getBookmarkURI: function (aId) BookmarksTestHelper._nodes[aId].uri,
    getItemTitle: function (aId) BookmarksTestHelper._nodes[aId].title,
    removeItem: function (aId) {
      delete BookmarksTestHelper._nodes[aId];

      // Simulate observer notification
      gStartView._changes.onItemRemoved(aId, gStartView._root);
    },
  },

  Node: function (aTitle, aId) {
    this.type = this.RESULT_TYPE_URI = 0;
    this.title = aTitle;
    this.itemId = aId;
    this.uri = "http://" + aTitle + ".com.br";
    this.pinned = true
  },

  _nodes: null,
  createNodes: function (aMany) {
    this._nodes = {};
    for (let i=0; i<aMany; i++) {
      this._nodes[i] = new this.Node("Mock-Bookmark" + i, i);
    }
  },

  _originalPinHelper: null,
  MockPinHelper: {
    isPinned: function (aItem) BookmarksTestHelper._nodes[aItem].pinned,
    setUnpinned: function (aItem) BookmarksTestHelper._nodes[aItem].pinned = false,
    setPinned: function (aItem) BookmarksTestHelper._nodes[aItem].pinned = true,
  },

  _originalUpdateFavicon: null,
  setup: function setup() {
    // Just enough items so that there will be one less then the limit
    // after removing 4 items.
    this.createNodes(gStartView._limit + 3);

    this._originalNavHistoryService = gStartView._navHistoryService;
    gStartView._navHistoryService = this.MockNavHistoryService;

    this._originalBookmarkService = gStartView._bookmarkService;
    gStartView._bookmarkService= this.MockBookmarkService;

    this._originalPinHelper = gStartView._pinHelper;
    gStartView._pinHelper = this.MockPinHelper;

    this._originalUpdateFavicon = gStartView._updateFavicon;
    gStartView._updateFavicon = function () {};

    gStartView.clearBookmarks();
    gStartView.getBookmarks();
  },

  restore: function () {
    gStartView._navHistoryService = this._originalNavHistoryService;
    gStartView._bookmarkService= this._originalBookmarkService;
    gStartView._pinHelper = this._originalPinHelper;
    gStartView._updateFavicon = this._originalUpdateFavicon;

    gStartView.clearBookmarks();
    gStartView.getBookmarks();
  }
};

gTests.push({
  desc: "Test bookmarks StartUI unpin",
  setUp: setup,
  tearDown: tearDown,
  run: function testBookmarksStartUnpin() {
    let unpinButton = document.getElementById("unpin-selected-button");

    // --------- unpin item 2

    let item = gStartView._getItemForBookmarkId(2);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    ok(!unpinButton.hidden, "Unpin button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    unpinButton.click();
    yield promise;

    item = gStartView._getItemForBookmarkId(2);

    ok(!item, "Item not in grid");
    ok(!gStartView._pinHelper.isPinned(2), "Item unpinned");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    // --------- unpin multiple items

    let item1 = gStartView._getItemForBookmarkId(0);
    let item2 = gStartView._getItemForBookmarkId(5);
    let item3 = gStartView._getItemForBookmarkId(12);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item1, 10, 10);
    sendContextMenuClickToElement(window, item2, 10, 10);
    sendContextMenuClickToElement(window, item3, 10, 10);
    yield promise;

    ok(!unpinButton.hidden, "Unpin button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(unpinButton, 10, 10, {}, window);
    yield promise;

    item1 = gStartView._getItemForBookmarkId(0);
    item2 = gStartView._getItemForBookmarkId(5);
    item3 = gStartView._getItemForBookmarkId(12);

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(!gStartView._pinHelper.isPinned(0) && !gStartView._pinHelper.isPinned(5) && !gStartView._pinHelper.isPinned(12) , "Items unpinned");
    ok(gStartView._set.itemCount === gStartView._limit - 1, "Grid repopulated");
  }
});

gTests.push({
  desc: "Test bookmarks StartUI delete",
  setUp: setup,
  tearDown: tearDown,
  run: function testBookmarksStartDelete() {
    let restoreButton = document.getElementById("restore-selected-button");
    let deleteButton = document.getElementById("delete-selected-button");

    // --------- delete item 2 and restore

    let item = gStartView._getItemForBookmarkId(2);
    let initialLocation = gStartView._set.getIndexOfItem(item);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    ok(!deleteButton.hidden, "Delete button is visible.");

    let promise = waitForCondition(() => !restoreButton.hidden);
    EventUtils.synthesizeMouse(deleteButton, 10, 10, {}, window);
    yield promise;

    item = gStartView._getItemForBookmarkId(2);

    ok(!item, "Item not in grid");
    ok(BookmarksTestHelper._nodes[2], "Item not deleted yet");
    ok(!restoreButton.hidden, "Restore button is visible.");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(restoreButton, 10, 10, {}, window);
    yield promise;

    item = gStartView._getItemForBookmarkId(2);
    ok(item, "Item back in grid");
    ok(gStartView._set.getIndexOfItem(item) === initialLocation, "Back in same position.");

    // --------- delete item 2 for realz

    let item = gStartView._getItemForBookmarkId(2);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    ok(!deleteButton.hidden, "Delete button is visible.");

    let promise = waitForCondition(() => !restoreButton.hidden);
    EventUtils.synthesizeMouse(deleteButton, 10, 10, {}, window);
    yield promise;

    item = gStartView._getItemForBookmarkId(2);

    ok(!item, "Item not in grid");
    ok(BookmarksTestHelper._nodes[2], "Item not deleted yet");
    ok(!restoreButton.hidden, "Restore button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    Elements.contextappbar.dismiss();
    yield promise;

    item = gStartView._getItemForBookmarkId(2);

    ok(!item, "Item not in grid");
    ok(!BookmarksTestHelper._nodes[2], "Item RIP");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    // --------- delete multiple items and restore

    let item1 = gStartView._getItemForBookmarkId(0);
    let item2 = gStartView._getItemForBookmarkId(5);
    let item3 = gStartView._getItemForBookmarkId(12);

    let initialLocation1 = gStartView._set.getIndexOfItem(item1);
    let initialLocation2 = gStartView._set.getIndexOfItem(item2);
    let initialLocation3 = gStartView._set.getIndexOfItem(item3);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item1, 10, 10);
    sendContextMenuClickToElement(window, item2, 10, 10);
    sendContextMenuClickToElement(window, item3, 10, 10);
    yield promise;

    ok(!deleteButton.hidden, "Delete button is visible.");

    let promise = waitForCondition(() => !restoreButton.hidden);
    EventUtils.synthesizeMouse(deleteButton, 10, 10, {}, window);
    yield promise;

    item1 = gStartView._getItemForBookmarkId(0);
    item2 = gStartView._getItemForBookmarkId(5);
    item3 = gStartView._getItemForBookmarkId(12);

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(BookmarksTestHelper._nodes[0] && BookmarksTestHelper._nodes[5] && BookmarksTestHelper._nodes[12],
      "Items not deleted yet");
    ok(!restoreButton.hidden, "Restore button is visible.");
    ok(gStartView._set.itemCount === gStartView._limit - 1, "Grid repopulated");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(restoreButton, 10, 10, {}, window);
    yield promise;

    item1 = gStartView._getItemForBookmarkId(0);
    item2 = gStartView._getItemForBookmarkId(5);
    item3 = gStartView._getItemForBookmarkId(12);

    ok(item1 && item2 && item3, "Items are back in grid");
    ok(gStartView._set.getIndexOfItem(item1) === initialLocation1 &&
      gStartView._set.getIndexOfItem(item2) === initialLocation2 &&
      gStartView._set.getIndexOfItem(item3) === initialLocation3, "Items back in the same position.");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    // --------- delete multiple items for good

    let item1 = gStartView._getItemForBookmarkId(0);
    let item2 = gStartView._getItemForBookmarkId(5);
    let item3 = gStartView._getItemForBookmarkId(12);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item1, 10, 10);
    sendContextMenuClickToElement(window, item2, 10, 10);
    sendContextMenuClickToElement(window, item3, 10, 10);
    yield promise;

    yield waitForCondition(() => !deleteButton.hidden);

    ok(!deleteButton.hidden, "Delete button is visible.");

    let promise = waitForCondition(() => !restoreButton.hidden);
    EventUtils.synthesizeMouse(deleteButton, 10, 10, {}, window);
    yield promise;

    item1 = gStartView._getItemForBookmarkId(0);
    item2 = gStartView._getItemForBookmarkId(5);
    item3 = gStartView._getItemForBookmarkId(12);

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(BookmarksTestHelper._nodes[0] && BookmarksTestHelper._nodes[5] && BookmarksTestHelper._nodes[12],
      "Items not deleted yet");
    ok(!restoreButton.hidden, "Restore button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    Elements.contextappbar.dismiss();
    yield promise;

    item1 = gStartView._getItemForBookmarkId(0);
    item2 = gStartView._getItemForBookmarkId(5);
    item3 = gStartView._getItemForBookmarkId(12);

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(!BookmarksTestHelper._nodes[0] && !BookmarksTestHelper._nodes[5] && !BookmarksTestHelper._nodes[12],
      "Items are gone");
    ok(gStartView._set.itemCount === gStartView._limit - 1, "Grid repopulated");
  }
});
