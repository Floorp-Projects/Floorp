/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the bookmarks toolbar overflow.
 */

var gToolbar = document.getElementById("PersonalToolbar");
var gChevron = document.getElementById("PlacesChevron");

const BOOKMARKS_COUNT = 250;

add_setup(async function () {
  let wasCollapsed = gToolbar.collapsed;
  await PlacesUtils.bookmarks.eraseEverything();

  // Add bookmarks.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: Array(BOOKMARKS_COUNT)
      .fill("")
      .map((_, i) => ({ url: `http://test.places.y${i}/` })),
  });

  // Toggle the bookmarks toolbar so that we start from a stable situation and
  // are not affected by all bookmarks removal.
  await toggleToolbar(false);
  await toggleToolbar(true);

  registerCleanupFunction(async () => {
    if (wasCollapsed) {
      await toggleToolbar(false);
    }
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_overflow() {
  // Check that the overflow chevron is visible.
  Assert.ok(!gChevron.collapsed, "The overflow chevron should be visible");
  let children = getPlacesChildren();
  Assert.ok(
    children.length < BOOKMARKS_COUNT,
    "Not all the nodes should be built by default"
  );
  let visibleNodes = [];
  for (let node of children) {
    if (getComputedStyle(node).visibility == "visible") {
      visibleNodes.push(node);
    }
  }
  Assert.ok(
    visibleNodes.length < children.length,
    `The number of visible nodes (${visibleNodes.length}) should be smaller than the number of built nodes (${children.length})`
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
  await test_index("First non-built node", children.length, undefined);
  await test_index("Later non-built node", children.length + 1, undefined);

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
    children.length - 1,
    "visible",
    "hidden"
  );
  await test_move_index(
    "Move node from fist visible to first non built",
    0,
    children.length,
    "visible",
    undefined
  );
});

add_task(async function test_separator_first() {
  await toggleToolbar(false);
  // Check that if a separator is the first node, we still calculate overflow
  // properly.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    index: 0,
  });
  await toggleToolbar(true, 2);

  let children = getPlacesChildren();
  Assert.greater(children.length, 2, "Multiple elements are visible");
  Assert.equal(
    children[1]._placesNode.uri,
    "http://test.places.y0/",
    "Found the first bookmark"
  );
  Assert.equal(
    getComputedStyle(children[1]).visibility,
    "visible",
    "The first bookmark is visible"
  );

  await PlacesUtils.bookmarks.remove(bm);
});

add_task(async function test_newWindow_noOverflow() {
  info(
    "Check toolbar in a new widow when it was already visible and not overflowed"
  );
  Assert.ok(!gToolbar.collapsed, "Toolbar is not collapsed in original window");
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
    await TestUtils.waitForCondition(() => {
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
  let children = getPlacesChildren();
  let originalLen = children.length;
  let nodeExisted = children.length > index;
  let previousNodeIsVisible =
    nodeExisted &&
    getComputedStyle(children[index - 1]).visibility == "visible";
  let promise = promiseUpdateVisibility(
    expected == "visible" || previousNodeIsVisible
  );

  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://test.places.added/",
    index,
  });
  Assert.equal(bm.index, index, "Sanity check the bookmark index");
  await promise;
  children = getPlacesChildren();

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
      getComputedStyle(children[index]).visibility,
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
  promise = promiseUpdateVisibility(expected == "visible");

  await PlacesUtils.bookmarks.remove(bm);
  await promise;
  children = getPlacesChildren();

  if (expected && nodeExisted) {
    Assert.equal(
      children[index]._placesNode.uri,
      `http://test.places.y${index}/`,
      "Found the previous bookmark at the expected position"
    );
    Assert.equal(
      getComputedStyle(children[index]).visibility,
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
  let children = getPlacesChildren();
  let originalLen = children.length;
  let movedGuid = children[fromIndex]._placesNode.bookmarkGuid;
  let existingGuid = children[toIndex]
    ? children[toIndex]._placesNode.bookmarkGuid
    : null;
  let existingIndex = fromIndex < toIndex ? toIndex - 1 : toIndex + 1;

  Assert.equal(
    getComputedStyle(children[fromIndex]).visibility,
    original,
    `The bookmark node should be ${original}`
  );
  let promise = promiseUpdateVisibility(
    original == "visible" || expected == "visible"
  );

  await PlacesUtils.bookmarks.update({
    guid: movedGuid,
    index: toIndex,
  });
  await promise;
  children = getPlacesChildren();

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
      getComputedStyle(children[toIndex]).visibility,
      expected,
      `The destination bookmark node should be ${expected}`
    );
  }
  Assert.equal(
    getComputedStyle(children[fromIndex]).visibility,
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
  promise = promiseUpdateVisibility(
    original == "visible" || expected == "visible"
  );

  await PlacesUtils.bookmarks.update({
    guid: movedGuid,
    index: fromIndex,
  });
  await promise;
  children = getPlacesChildren();

  Assert.equal(
    children[fromIndex]._placesNode.bookmarkGuid,
    movedGuid,
    "Found the moved bookmark at the expected position"
  );
  if (expected) {
    Assert.equal(
      getComputedStyle(children[toIndex]).visibility,
      expected,
      `The bookmark node should be ${expected}`
    );
  }
  Assert.equal(
    getComputedStyle(children[fromIndex]).visibility,
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
  await toggleToolbar(false);
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
  await toggleToolbar(true, 2);

  let children = getPlacesChildren();
  Assert.equal(children.length, 2, "The expected elements are visible");
  Assert.equal(
    getComputedStyle(children[0]).visibility,
    "visible",
    "The first bookmark is visible"
  );
  Assert.equal(
    getComputedStyle(children[1]).visibility,
    "visible",
    "The second bookmark is visible"
  );

  await toggleToolbar(false);
  await PlacesUtils.bookmarks.eraseEverything();
});

/**
 * If the passed-in condition is fulfilled, awaits for the toolbar nodes
 * visibility to have been updated.
 *
 * @param {boolean} [condition] Awaits for visibility only if this condition is true.
 * @returns {Promise} resolved when the condition is not fulfilled or the
 *          visilibily update happened.
 */
function promiseUpdateVisibility(condition = true) {
  if (condition) {
    return BrowserTestUtils.waitForEvent(
      gToolbar,
      "BookmarksToolbarVisibilityUpdated"
    );
  }
  return Promise.resolve();
}

/**
 * Returns an array of toolbar children that are Places nodes, ignoring things
 * like the chevron or other additional buttons.
 *
 * @returns {Array} An array of Places element nodes.
 */
function getPlacesChildren() {
  return Array.prototype.filter.call(
    document.getElementById("PlacesToolbarItems").children,
    c => c._placesNode?.itemId
  );
}

/**
 * Toggles the toolbar on or off.
 *
 * @param {boolean} show Whether to show or hide the toolbar.
 * @param {number} [expectedMinChildCount] Optional number of Places nodes that
 *        should be visible on the toolbar.
 */
async function toggleToolbar(show, expectedMinChildCount = 0) {
  let promiseReady = Promise.resolve();
  if (show) {
    promiseReady = promiseUpdateVisibility();
  }

  await promiseSetToolbarVisibility(gToolbar, show);
  await promiseReady;

  if (show) {
    if (getPlacesChildren().length < expectedMinChildCount) {
      await new Promise(resolve => {
        info("Waiting for bookmark elements to appear");
        let mut = new MutationObserver(mutations => {
          let children = getPlacesChildren();
          info(`${children.length} bookmark elements appeared`);
          if (children.length >= expectedMinChildCount) {
            resolve();
            mut.disconnect();
          }
        });
        mut.observe(document.getElementById("PlacesToolbarItems"), {
          childList: true,
        });
      });
    }
  }
}
