/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the bookmarks toolbar overflow.
 */

var gToolbar = document.getElementById("PersonalToolbar");
var gToolbarContent = document.getElementById("PlacesToolbarItems");
var gChevron = document.getElementById("PlacesChevron");

const BOOKMARKS_COUNT = 250;

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();

  // Add bookmarks.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: Array(BOOKMARKS_COUNT)
      .fill("")
      .map((_, i) => ({ url: `http://test.places.${i}/` })),
  });

  // Uncollapse the personal toolbar if needed.
  let wasCollapsed = gToolbar.collapsed;
  Assert.ok(wasCollapsed, "The toolbar is collapsed by default");
  if (wasCollapsed) {
    let promiseReady = BrowserTestUtils.waitForEvent(
      gToolbar,
      "BookmarksToolbarVisibilityUpdated"
    );
    await promiseSetToolbarVisibility(gToolbar, true);
    await promiseReady;
  }
  registerCleanupFunction(async () => {
    if (wasCollapsed) {
      await promiseSetToolbarVisibility(gToolbar, false);
    }
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_overflow() {
  // Check that the overflow chevron is visible.
  Assert.ok(!gChevron.collapsed, "The overflow chevron should be visible");
  Assert.ok(
    gToolbarContent.children.length < BOOKMARKS_COUNT,
    "Not all the nodes should be built by default"
  );
  let visibleNodes = [];
  for (let node of gToolbarContent.children) {
    if (node.style.visibility == "visible") {
      visibleNodes.push(node);
    }
  }
  Assert.ok(
    visibleNodes.length < gToolbarContent.children.length,
    `The number of visible nodes (${
      visibleNodes.length
    }) should be smaller than the number of built nodes (${
      gToolbarContent.children.length
    })`
  );

  await test_index(
    "Node at the last visible index",
    visibleNodes.length - 1,
    "visible"
  );
  await test_index(
    "Node at the first invisible index",
    visibleNodes.length,
    "hidden"
  );
  await test_index(
    "First non-built node",
    gToolbarContent.children.length,
    undefined
  );
  await test_index(
    "Later non-built node",
    gToolbarContent.children.length + 1,
    undefined
  );

  await test_move_index(
    "Move node from last visible to first hidden",
    visibleNodes.length - 1,
    visibleNodes.length,
    "visible",
    "hidden"
  );
  await test_move_index(
    "Move node from fist visible to last built",
    0,
    gToolbarContent.children.length - 1,
    "visible",
    "hidden"
  );
  await test_move_index(
    "Move node from fist visible to first non built",
    0,
    gToolbarContent.children.length,
    "visible",
    undefined
  );
});

add_task(async function test_separator_first() {
  // Check that if a separator is the first node, we still calculate overflow
  // properly.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    index: 0,
  });
  // Hide and show the toolbar to cause a rebuild.
  let promiseReady = BrowserTestUtils.waitForEvent(
    gToolbar,
    "BookmarksToolbarVisibilityUpdated"
  );
  await promiseSetToolbarVisibility(gToolbar, false);
  await promiseReady;
  promiseReady = BrowserTestUtils.waitForEvent(
    gToolbar,
    "BookmarksToolbarVisibilityUpdated"
  );
  await promiseSetToolbarVisibility(gToolbar, true);
  await promiseReady;

  let children = gToolbarContent.children;
  Assert.greater(children.length, 2, "Multiple elements are visible");
  Assert.equal(
    children[1]._placesNode.uri,
    "http://test.places.0/",
    "Found the first bookmark"
  );
  Assert.equal(
    children[1].style.visibility,
    "visible",
    "The first bookmark is visible"
  );

  await PlacesUtils.bookmarks.remove(bm);
});

add_task(async function test_newWindow_noOverflow() {
  info(
    "Check toolbar in a new widow when it was already visible and not overflowed"
  );
  await PlacesUtils.bookmarks.eraseEverything();
  // Add a single bookmark.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://toolbar.overflow/",
    title: "Example",
  });
  // Add a favicon for the bookmark.
  let favicon =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAA" +
    "AAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";
  await PlacesTestUtils.addFavicons(
    new Map([["http://toolbar.overflow/", favicon]])
  );

  let win = await BrowserTestUtils.openNewBrowserWindow();
  try {
    let toolbar = win.document.getElementById("PersonalToolbar");
    Assert.ok(!toolbar.collapsed, "The toolbar is not collapsed");
    let content = win.document.getElementById("PlacesToolbarItems");
    await BrowserTestUtils.waitForCondition(() => {
      return (
        content.children.length == 1 &&
        content.children[0].hasAttribute("image")
      );
    });
    let chevron = win.document.getElementById("PlacesChevron");
    Assert.ok(chevron.collapsed, "The chevron should be collapsed");
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});

async function test_index(desc, index, expected) {
  info(desc);
  let children = gToolbarContent.children;
  let originalLen = children.length;
  let nodeExisted = children.length > index;
  let previousNodeIsVisible =
    nodeExisted && children[index - 1].style.visibility == "visible";
  let promiseUpdateVisibility =
    expected == "visible" || previousNodeIsVisible
      ? BrowserTestUtils.waitForEvent(
          gToolbar,
          "BookmarksToolbarVisibilityUpdated"
        )
      : Promise.resolve();
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://test.places.added/",
    index,
  });
  Assert.equal(bm.index, index, "Sanity check the bookmark index");
  await promiseUpdateVisibility;

  if (!expected) {
    Assert.ok(
      children.length <= index,
      "The new node should not have been added"
    );
  } else {
    Assert.equal(
      children[index]._placesNode.bookmarkGuid,
      bm.guid,
      "Found the added bookmark at the expected position"
    );
    Assert.equal(
      children[index].style.visibility,
      expected,
      `The bookmark node should be ${expected}`
    );
  }
  Assert.equal(
    children.length,
    originalLen,
    "Number of built nodes should stay the same"
  );

  info("Remove the node");
  promiseUpdateVisibility =
    expected == "visible"
      ? BrowserTestUtils.waitForEvent(
          gToolbar,
          "BookmarksToolbarVisibilityUpdated"
        )
      : Promise.resolve();
  await PlacesUtils.bookmarks.remove(bm);
  await promiseUpdateVisibility;

  if (expected && nodeExisted) {
    Assert.equal(
      children[index]._placesNode.uri,
      `http://test.places.${index}/`,
      "Found the previous bookmark at the expected position"
    );
    Assert.equal(
      children[index].style.visibility,
      expected,
      `The bookmark node should be ${expected}`
    );
  }
  Assert.equal(
    children.length,
    originalLen,
    "Number of built nodes should stay the same"
  );
}

async function test_move_index(desc, fromIndex, toIndex, original, expected) {
  info(desc);
  let children = gToolbarContent.children;
  let originalLen = children.length;
  let movedGuid = children[fromIndex]._placesNode.bookmarkGuid;
  let existingGuid = children[toIndex]
    ? children[toIndex]._placesNode.bookmarkGuid
    : null;
  let existingIndex = fromIndex < toIndex ? toIndex - 1 : toIndex + 1;

  Assert.equal(
    children[fromIndex].style.visibility,
    original,
    `The bookmark node should be ${original}`
  );
  let promiseUpdateVisibility =
    original == "visible" || expected == "visible"
      ? BrowserTestUtils.waitForEvent(
          gToolbar,
          "BookmarksToolbarVisibilityUpdated"
        )
      : Promise.resolve();
  await PlacesUtils.bookmarks.update({
    guid: movedGuid,
    index: toIndex,
  });
  await promiseUpdateVisibility;

  if (!expected) {
    Assert.ok(
      children.length <= toIndex,
      "Node in the new position is not expected"
    );
    Assert.ok(
      children[originalLen - 1],
      "We should keep number of built nodes consistent"
    );
  } else {
    Assert.equal(
      children[toIndex]._placesNode.bookmarkGuid,
      movedGuid,
      "Found the moved bookmark at the expected position"
    );
    Assert.equal(
      children[toIndex].style.visibility,
      expected,
      `The destination bookmark node should be ${expected}`
    );
  }
  Assert.equal(
    children[fromIndex].style.visibility,
    original,
    `The origin bookmark node should be ${original}`
  );
  if (existingGuid) {
    Assert.equal(
      children[existingIndex]._placesNode.bookmarkGuid,
      existingGuid,
      "Found the pushed away bookmark at the expected position"
    );
  }
  Assert.equal(
    children.length,
    originalLen,
    "Number of built nodes should stay the same"
  );

  info("Moving back the node");
  promiseUpdateVisibility =
    original == "visible" || expected == "visible"
      ? BrowserTestUtils.waitForEvent(
          gToolbar,
          "BookmarksToolbarVisibilityUpdated"
        )
      : Promise.resolve();
  await PlacesUtils.bookmarks.update({
    guid: movedGuid,
    index: fromIndex,
  });
  await promiseUpdateVisibility;

  Assert.equal(
    children[fromIndex]._placesNode.bookmarkGuid,
    movedGuid,
    "Found the moved bookmark at the expected position"
  );
  if (expected) {
    Assert.equal(
      children[toIndex].style.visibility,
      expected,
      `The bookmark node should be ${expected}`
    );
  }
  Assert.equal(
    children[fromIndex].style.visibility,
    original,
    `The bookmark node should be ${original}`
  );
  if (existingGuid) {
    Assert.equal(
      children[toIndex]._placesNode.bookmarkGuid,
      existingGuid,
      "Found the pushed away bookmark at the expected position"
    );
  }
  Assert.equal(
    children.length,
    originalLen,
    "Number of built nodes should stay the same"
  );
}

add_task(async function test_separator_first() {
  // Check that if there are only separators, we still show nodes properly.
  await PlacesUtils.bookmarks.eraseEverything();

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    index: 0,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    index: 0,
  });
  // Hide and show the toolbar to cause a rebuild.
  let promiseReady = BrowserTestUtils.waitForEvent(
    gToolbar,
    "BookmarksToolbarVisibilityUpdated"
  );
  await promiseSetToolbarVisibility(gToolbar, false);
  await promiseReady;
  promiseReady = BrowserTestUtils.waitForEvent(
    gToolbar,
    "BookmarksToolbarVisibilityUpdated"
  );
  await promiseSetToolbarVisibility(gToolbar, true);
  await promiseReady;

  let children = gToolbarContent.children;
  Assert.equal(children.length, 2, "The expected elements are visible");
  Assert.equal(
    children[0].style.visibility,
    "visible",
    "The first bookmark is visible"
  );
  Assert.equal(
    children[1].style.visibility,
    "visible",
    "The second bookmark is visible"
  );

  await PlacesUtils.bookmarks.eraseEverything();
});
