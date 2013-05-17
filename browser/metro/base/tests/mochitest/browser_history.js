// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let gStartView = HistoryStartView._view;
let gPanelView = HistoryPanelView._view;

function test() {
  runTests();
}

function setup() {
  PanelUI.hide();
  HistoryTestHelper.setup();

  if (StartUI.isStartPageVisible)
    return;

  yield addTab("about:start");

  yield waitForCondition(() => StartUI.isStartPageVisible);

  yield hideContextUI();
}

function tearDown() {
  PanelUI.hide();
  HistoryTestHelper.restore();
}

var HistoryTestHelper = {
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
            return Object.keys(HistoryTestHelper._nodes).length;
          },

          getChild: function (aIndex) HistoryTestHelper._nodes[Object.keys(HistoryTestHelper._nodes)[aIndex]]
        }
      }
    }
  },

  _originalHistoryService: null,
  MockHistoryService: {
    removePage: function (aURI) {
      delete HistoryTestHelper._nodes[aURI.spec];

      // Simulate observer notification
      gStartView.onDeleteURI(aURI);
      gPanelView.onDeleteURI(aURI);
    },
  },

  Node: function (aTitle, aURISpec) {
    this.title = aTitle;
    this.uri = aURISpec;
    this.pinned = true
  },

  _nodes: null,
  createNodes: function (aMany) {
    this._nodes = {};
    for (let i=0; i<aMany; i++) {
      let title = "mock-history-" + i;
      let uri = "http://" + title + ".com.br/";

      this._nodes[uri] = new this.Node(title, uri);
    }
  },

  _originalPinHelper: null,
  MockPinHelper: {
    isPinned: function (aItem) HistoryTestHelper._nodes[aItem].pinned,
    setUnpinned: function (aItem) HistoryTestHelper._nodes[aItem].pinned = false,
    setPinned: function (aItem) HistoryTestHelper._nodes[aItem].pinned = true,
  },

  setup: function setup() {
    // Just enough items so that there will be one less then the limit
    // after removing 4 items.
    this.createNodes(gStartView._limit + 3);

    this._originalNavHistoryService = gStartView._navHistoryService;
    gStartView._navHistoryService = this.MockNavHistoryService;
    gPanelView._navHistoryService = this.MockNavHistoryService;

    this._originalHistoryService = gStartView._historyService;
    gStartView._historyService= this.MockHistoryService;
    gPanelView._historyService= this.MockHistoryService;

    this._originalPinHelper = gStartView._pinHelper;
    gStartView._pinHelper = this.MockPinHelper;
    gPanelView._pinHelper = this.MockPinHelper;

    gStartView._set.clearAll();
    gStartView.populateGrid();
    gPanelView._set.clearAll();
    gPanelView.populateGrid();
  },

  restore: function () {
    gStartView._navHistoryService = this._originalNavHistoryService;
    gStartView._historyService= this._originalHistoryService;
    gStartView._pinHelper = this._originalPinHelper;

    gPanelView._navHistoryService = this._originalNavHistoryService;
    gPanelView._historyService= this._originalHistoryService;
    gPanelView._pinHelper = this._originalPinHelper;

    gStartView._set.clearAll();
    gStartView.populateGrid();
    gPanelView._set.clearAll();
    gPanelView.populateGrid();
  }
};

function uriFromIndex(aIndex) {
  return "http://mock-history-" + aIndex + ".com.br/"
}

gTests.push({
  desc: "Test history StartUI unpin",
  setUp: setup,
  tearDown: tearDown,
  run: function testHistoryStartUnpin() {
    let unpinButton = document.getElementById("unpin-selected-button");

    // --------- unpin item 2

    let item = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    ok(!unpinButton.hidden, "Unpin button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    unpinButton.click();
    yield promise;

    item = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    ok(!item, "Item not in grid");
    ok(!gStartView._pinHelper.isPinned(uriFromIndex(2)), "Item unpinned");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    // --------- unpin multiple items

    let item1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    let item2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    let item3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item1, 10, 10);
    sendContextMenuClickToElement(window, item2, 10, 10);
    sendContextMenuClickToElement(window, item3, 10, 10);
    yield promise;

    ok(!unpinButton.hidden, "Unpin button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(unpinButton, 10, 10, {}, window);
    yield promise;

    item1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(!gStartView._pinHelper.isPinned(uriFromIndex(0)) && !gStartView._pinHelper.isPinned(uriFromIndex(5)) && !gStartView._pinHelper.isPinned(uriFromIndex(12)) , "Items unpinned");
    ok(gStartView._set.itemCount === gStartView._limit - 1, "Grid repopulated");
  }
});

gTests.push({
  desc: "Test history StartUI delete",
  setUp: setup,
  tearDown: tearDown,
  run: function testHistoryStartDelete() {
    let restoreButton = document.getElementById("restore-selected-button");
    let deleteButton = document.getElementById("delete-selected-button");

    // --------- delete item 2 and restore

    let item = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];
    let initialLocation = gStartView._set.getIndexOfItem(item);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    ok(!deleteButton.hidden, "Delete button is visible.");

    let promise = waitForCondition(() => !restoreButton.hidden);
    EventUtils.synthesizeMouse(deleteButton, 10, 10, {}, window);
    yield promise;

    item = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    ok(!item, "Item not in grid");
    ok(HistoryTestHelper._nodes[uriFromIndex(2)], "Item not deleted yet");
    ok(!restoreButton.hidden, "Restore button is visible.");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(restoreButton, 10, 10, {}, window);
    yield promise;

    item = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];
    ok(item, "Item back in grid");
    ok(gStartView._set.getIndexOfItem(item) === initialLocation, "Back in same position.");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    // --------- delete item 2 for realz

    let item = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    yield waitForCondition(() => !deleteButton.hidden);

    ok(!deleteButton.hidden, "Delete button is visible.");

    let promise = waitForCondition(() => !restoreButton.hidden);
    EventUtils.synthesizeMouse(deleteButton, 10, 10, {}, window);
    yield promise;

    item = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    ok(!item, "Item not in grid");
    ok(HistoryTestHelper._nodes[uriFromIndex(2)], "Item not deleted yet");
    ok(!restoreButton.hidden, "Restore button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    Elements.contextappbar.dismiss();
    yield promise;

    item = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    ok(!item, "Item not in grid");
    ok(!HistoryTestHelper._nodes[uriFromIndex(2)], "Item RIP");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    // --------- delete multiple items and restore

    let item1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    let item2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    let item3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    let initialLocation1 = gStartView._set.getIndexOfItem(item1);
    let initialLocation2 = gStartView._set.getIndexOfItem(item2);
    let initialLocation3 = gStartView._set.getIndexOfItem(item3);

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

    item1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(HistoryTestHelper._nodes[uriFromIndex(0)] && HistoryTestHelper._nodes[uriFromIndex(5)] && HistoryTestHelper._nodes[uriFromIndex(12)],
      "Items not deleted yet");
    ok(!restoreButton.hidden, "Restore button is visible.");
    ok(gStartView._set.itemCount === gStartView._limit - 1, "Grid repopulated");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(restoreButton, 10, 10, {}, window);
    yield promise;

    item1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(item1 && item2 && item3, "Items are back in grid");
    ok(gStartView._set.getIndexOfItem(item1) === initialLocation1 &&
      gStartView._set.getIndexOfItem(item2) === initialLocation2 &&
      gStartView._set.getIndexOfItem(item3) === initialLocation3, "Items back in the same position.");
    ok(gStartView._set.itemCount === gStartView._limit, "Grid repopulated");

    // --------- delete multiple items for good

    let item1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    let item2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    let item3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

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

    item1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(HistoryTestHelper._nodes[uriFromIndex(0)] && HistoryTestHelper._nodes[uriFromIndex(5)] && HistoryTestHelper._nodes[uriFromIndex(12)],
      "Items not deleted yet");
    ok(!restoreButton.hidden, "Restore button is visible.");
    ok(gStartView._set.itemCount === gStartView._limit - 1, "Grid repopulated");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    Elements.contextappbar.dismiss();
    yield promise;

    item1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(!HistoryTestHelper._nodes[uriFromIndex(0)] && !HistoryTestHelper._nodes[uriFromIndex(5)] && !HistoryTestHelper._nodes[uriFromIndex(12)],
      "Items are gone");
    ok(gStartView._set.itemCount === gStartView._limit - 1, "Grid repopulated");
  }
});

gTests.push({
  desc: "Test history PanelUI unpin",
  setUp: setup,
  tearDown: tearDown,
  run: function testHistoryPanelUnpin() {
    PanelUI.show('history-container');

    let pinButton = document.getElementById("pin-selected-button");
    let unpinButton = document.getElementById("unpin-selected-button");

    // --------- unpin item 2

    let item = gPanelView._set.getItemsByUrl(uriFromIndex(2))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    yield waitForCondition(() => !unpinButton.hidden);

    ok(!unpinButton.hidden, "Unpin button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(unpinButton, 10, 10, {}, window);
    yield promise;

    item = gPanelView._set.getItemsByUrl(uriFromIndex(2))[0];
    let startItem = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    ok(item, "Item is in grid");
    ok(!startItem, "Item not in start grid");
    ok(!gPanelView._pinHelper.isPinned(uriFromIndex(2)), "Item unpinned");

    // --------- unpin multiple items

    let item1 = gPanelView._set.getItemsByUrl(uriFromIndex(0))[0];
    let item2 = gPanelView._set.getItemsByUrl(uriFromIndex(5))[0];
    let item3 = gPanelView._set.getItemsByUrl(uriFromIndex(12))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item1, 10, 10);
    sendContextMenuClickToElement(window, item2, 10, 10);
    sendContextMenuClickToElement(window, item3, 10, 10);
    yield promise;

    ok(!unpinButton.hidden, "Unpin button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(unpinButton, 10, 10, {}, window);
    yield promise;

    item1 = gPanelView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gPanelView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gPanelView._set.getItemsByUrl(uriFromIndex(12))[0];
    let startItem1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    let startItem2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    let startItem3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(item1 && item2 && item3, "Items are in grid");
    ok(!startItem1 && !startItem2 && !startItem3, "Items are not in start grid");
    ok(!gPanelView._pinHelper.isPinned(uriFromIndex(0)) && !gPanelView._pinHelper.isPinned(uriFromIndex(5)) && !gPanelView._pinHelper.isPinned(uriFromIndex(12)) , "Items unpinned");

    // --------- pin item 2

    let item = gPanelView._set.getItemsByUrl(uriFromIndex(2))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    // Make sure app bar is updated
    yield waitForCondition(() => !pinButton.hidden);

    ok(!pinButton.hidden, "Pin button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(pinButton, 10, 10, {}, window);
    yield promise;

    item = gPanelView._set.getItemsByUrl(uriFromIndex(2))[0];
    let startItem = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    ok(item, "Item is in grid");
    ok(startItem, "Item is back in start grid");
    ok(gPanelView._pinHelper.isPinned(uriFromIndex(2)), "Item pinned");

    // --------- pin multiple items

    let item1 = gPanelView._set.getItemsByUrl(uriFromIndex(0))[0];
    let item2 = gPanelView._set.getItemsByUrl(uriFromIndex(5))[0];
    let item3 = gPanelView._set.getItemsByUrl(uriFromIndex(12))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item1, 10, 10);
    sendContextMenuClickToElement(window, item2, 10, 10);
    sendContextMenuClickToElement(window, item3, 10, 10);
    yield promise;

    // Make sure app bar is updated
    yield waitForCondition(() => !pinButton.hidden);

    ok(!pinButton.hidden, "pin button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(pinButton, 10, 10, {}, window);
    yield promise;

    item1 = gPanelView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gPanelView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gPanelView._set.getItemsByUrl(uriFromIndex(12))[0];
    let startItem1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    let startItem2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    let startItem3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(item1 && item2 && item3, "Items are in grid");
    ok(startItem1 && startItem2 && startItem3, "Items are back in start grid");
    ok(gPanelView._pinHelper.isPinned(uriFromIndex(0)) && gPanelView._pinHelper.isPinned(uriFromIndex(5)) && gPanelView._pinHelper.isPinned(uriFromIndex(12)) , "Items pinned");
  }
});

gTests.push({
  desc: "Test history PanelUI delete",
  setUp: setup,
  tearDown: tearDown,
  run: function testHistoryPanelDelete() {
    PanelUI.show('history-container');

    let restoreButton = document.getElementById("restore-selected-button");
    let deleteButton = document.getElementById("delete-selected-button");

    // --------- delete item 2

    let item = gPanelView._set.getItemsByUrl(uriFromIndex(2))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    ok(!deleteButton.hidden, "Delete button is visible.");

    let promise = waitForCondition(() => !restoreButton.hidden);
    EventUtils.synthesizeMouse(deleteButton, 10, 10, {}, window);
    yield promise;

    item = gPanelView._set.getItemsByUrl(uriFromIndex(2))[0];
    let startItem = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    ok(!item, "Item is not in grid");
    ok(startItem, "Item is not deleted from start grid yet");
    ok(HistoryTestHelper._nodes[uriFromIndex(2)], "Item exists");
    ok(!restoreButton.hidden, "Restore button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    Elements.contextappbar.dismiss();
    yield promise;

    item = gPanelView._set.getItemsByUrl(uriFromIndex(2))[0];
    startItem = gStartView._set.getItemsByUrl(uriFromIndex(2))[0];

    ok(!item, "Item gone from grid");
    ok(!startItem, "Item gone from start grid");
    ok(!HistoryTestHelper._nodes[uriFromIndex(2)], "Item RIP");

    // --------- delete multiple items

    let item1 = gPanelView._set.getItemsByUrl(uriFromIndex(0))[0];
    let item2 = gPanelView._set.getItemsByUrl(uriFromIndex(5))[0];
    let item3 = gPanelView._set.getItemsByUrl(uriFromIndex(12))[0];

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item1, 10, 10);
    sendContextMenuClickToElement(window, item2, 10, 10);
    sendContextMenuClickToElement(window, item3, 10, 10);
    yield promise;

    ok(!deleteButton.hidden, "Delete button is visible.");

    let promise = waitForCondition(() => !restoreButton.hidden);
    EventUtils.synthesizeMouse(deleteButton, 10, 10, {}, window);
    yield promise;

    item1 = gPanelView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gPanelView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gPanelView._set.getItemsByUrl(uriFromIndex(12))[0];
    let startItem1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    let startItem2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    let startItem3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(!restoreButton.hidden, "Restore button is visible.");
    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(startItem1 && startItem2 && startItem3, "Items are still in start grid");
    ok(HistoryTestHelper._nodes[uriFromIndex(0)] && HistoryTestHelper._nodes[uriFromIndex(5)] && HistoryTestHelper._nodes[uriFromIndex(12)],
      "Items not deleted yet");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    Elements.contextappbar.dismiss();
    yield promise;

    item1 = gPanelView._set.getItemsByUrl(uriFromIndex(0))[0];
    item2 = gPanelView._set.getItemsByUrl(uriFromIndex(5))[0];
    item3 = gPanelView._set.getItemsByUrl(uriFromIndex(12))[0];
    let startItem1 = gStartView._set.getItemsByUrl(uriFromIndex(0))[0];
    let startItem2 = gStartView._set.getItemsByUrl(uriFromIndex(5))[0];
    let startItem3 = gStartView._set.getItemsByUrl(uriFromIndex(12))[0];

    ok(!item1 && !item2 && !item3, "Items are gone from grid");
    ok(!startItem1 && !startItem2 && !startItem3, "Items are gone from start grid");
    ok(!HistoryTestHelper._nodes[uriFromIndex(0)] && !HistoryTestHelper._nodes[uriFromIndex(5)] && !HistoryTestHelper._nodes[uriFromIndex(12)],
      "Items are gone for good");
  }
});
