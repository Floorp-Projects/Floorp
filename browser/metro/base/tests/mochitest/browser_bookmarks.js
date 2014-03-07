// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let gStartView = null;

function test() {
  runTests();
}

function setup() {
  PanelUI.hide();

  if (!BrowserUI.isStartTabVisible) {
    let tab = yield addTab("about:start");
    gStartView = tab.browser.contentWindow.BookmarksStartView._view;

    yield waitForCondition(() => BrowserUI.isStartTabVisible);

    yield hideContextUI();
  }

  BookmarksTestHelper.setup();
}

function tearDown() {
  PanelUI.hide();
  BookmarksTestHelper.restore();
}

gTests.push({
  desc: "Test bookmarks StartUI hide",
  setUp: setup,
  tearDown: tearDown,
  run: function testBookmarksStartHide() {
    let hideButton = document.getElementById("hide-selected-button");

    // --------- hide item 2

    let item = gStartView._getItemForBookmarkId(2);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item, 10, 10);
    yield promise;

    ok(!hideButton.hidden, "Hide button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    hideButton.click();
    yield promise;

    item = gStartView._getItemForBookmarkId(2);

    ok(!item, "Item not in grid");
    ok(!gStartView._pinHelper.isPinned(2), "Item hidden");
    ok(gStartView._set.itemCount === gStartView.maxTiles, "Grid repopulated");

    // --------- hide multiple items

    let item1 = gStartView._getItemForBookmarkId(0);
    let item2 = gStartView._getItemForBookmarkId(5);
    let item3 = gStartView._getItemForBookmarkId(12);

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    sendContextMenuClickToElement(window, item1, 10, 10);
    sendContextMenuClickToElement(window, item2, 10, 10);
    sendContextMenuClickToElement(window, item3, 10, 10);
    yield promise;

    ok(!hideButton.hidden, "Hide button is visible.");

    let promise = waitForEvent(Elements.contextappbar, "transitionend", null, Elements.contextappbar);
    EventUtils.synthesizeMouse(hideButton, 10, 10, {}, window);
    yield promise;

    item1 = gStartView._getItemForBookmarkId(0);
    item2 = gStartView._getItemForBookmarkId(5);
    item3 = gStartView._getItemForBookmarkId(12);

    ok(!item1 && !item2 && !item3, "Items are not in grid");
    ok(!gStartView._pinHelper.isPinned(0) && !gStartView._pinHelper.isPinned(5) && !gStartView._pinHelper.isPinned(12) , "Items hidden");
    ok(gStartView._set.itemCount === gStartView.maxTiles - 1, "Grid repopulated");
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
    ok(gStartView._set.itemCount === gStartView.maxTiles, "Grid repopulated");

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
    ok(gStartView._set.itemCount === gStartView.maxTiles, "Grid repopulated");

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
    ok(gStartView._set.itemCount === gStartView.maxTiles - 1, "Grid repopulated");

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
    ok(gStartView._set.itemCount === gStartView.maxTiles, "Grid repopulated");

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
    ok(gStartView._set.itemCount === gStartView.maxTiles - 1, "Grid repopulated");
  }
});
