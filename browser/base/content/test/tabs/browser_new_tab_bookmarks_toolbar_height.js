/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests that showing the bookmarks toolbar for new tabs only doesn't affect
// the view port height in background tabs.

add_task(async function () {
  registerCleanupFunction(() => {
    setToolbarVisibility(
      BookmarkingUI.toolbar,
      gBookmarksToolbarVisibility,
      false,
      false
    );
  });

  setToolbarVisibility(BookmarkingUI.toolbar, false, false, false);
  is(
    BookmarkingUI.toolbar.getAttribute("collapsed"),
    "true",
    "bookmarks toolbar is hidden"
  );

  let pageURL = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  );
  pageURL = `${pageURL}file_observe_height_changes.html`;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageURL);

  let expectedHeightChanges = 0;
  let assertHeightChanges = async msg => {
    let heightChanges = await ContentTask.spawn(
      tab.linkedBrowser,
      null,
      async args => {
        await new Promise(resolve => content.requestAnimationFrame(resolve));
        return content.document.body.innerText;
      }
    );
    is(heightChanges, expectedHeightChanges, msg);
  };
  let assertBmToolbarVisibility = (visible, msg) => {
    is(
      BookmarkingUI.toolbar.getAttribute("collapsed"),
      (!visible).toString(),
      "bookmarks toolbar collapsed attribute state"
    );
    if (visible) {
      isnot(
        BookmarkingUI.toolbar.getBoundingClientRect().height,
        0,
        "bookmarks toolbar should have a height"
      );
    } else {
      is(
        BookmarkingUI.toolbar.getBoundingClientRect().height,
        0,
        "bookmarks toolbar height should be 0"
      );
    }
  };

  setToolbarVisibility(BookmarkingUI.toolbar, true, false, false);
  assertBmToolbarVisibility(true, "bookmarks toolbar is visible");
  expectedHeightChanges++;
  assertHeightChanges(
    "height changes when showing the toolbar without the animation"
  );

  setToolbarVisibility(BookmarkingUI.toolbar, "newtab", false, false);
  assertBmToolbarVisibility(
    false,
    "bookmarks toolbar is hidden for non-new tab"
  );
  expectedHeightChanges++;
  assertHeightChanges(
    "height changes when hiding the toolbar without the animation"
  );

  info("Opening a new tab, making the previous tab non-selected");
  BrowserOpenTab();
  ok(!tab.selected, "non-new tab is in the background (not the selected tab)");
  assertBmToolbarVisibility(true, "bookmarks toolbar is visible for new tab");
  assertHeightChanges(
    "no additional height change in background tab when showing the bookmarks toolbar in new tab"
  );

  gBrowser.removeCurrentTab();
  gBrowser.removeTab(tab);
});
