/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that the a bookmark can be opened from the Library by mouse double click.
 */
"use strict";

const TEST_URL = "about:buildconfig";

add_task(async function test_open_bookmark_from_library() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: TEST_URL,
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  let gLibrary = await promiseLibrary("UnfiledBookmarks");

  registerCleanupFunction(async function () {
    await promiseLibraryClosed(gLibrary);
    await PlacesUtils.bookmarks.eraseEverything();
    await BrowserTestUtils.removeTab(tab);
  });

  let bmLibrary = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
  Assert.equal(bmLibrary.title, bm.title, "Found bookmark in the right pane");

  gLibrary.ContentTree.view.selectNode(bmLibrary);
  synthesizeClickOnSelectedTreeCell(gLibrary.ContentTree.view, {
    clickCount: 2,
  });

  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    TEST_URL
  );
  Assert.ok(true, "Expected tab was loaded");
});
