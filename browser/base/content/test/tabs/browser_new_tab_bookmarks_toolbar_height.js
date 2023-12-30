/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests that showing the bookmarks toolbar for new tabs only doesn't affect
// the view port height in background tabs.

let gHeightChanges = 0;
async function expectHeightChanges(tab, expectedNewHeightChanges, msg) {
  let contentObservedHeightChanges = await ContentTask.spawn(
    tab.linkedBrowser,
    null,
    async args => {
      await new Promise(resolve => content.requestAnimationFrame(resolve));
      return content.document.body.innerText;
    }
  );
  is(
    contentObservedHeightChanges - gHeightChanges,
    expectedNewHeightChanges,
    msg
  );
  gHeightChanges = contentObservedHeightChanges;
}

async function expectBmToolbarVisibilityChange(triggerFn, visible, msg) {
  let collapsedState = BrowserTestUtils.waitForMutationCondition(
    BookmarkingUI.toolbar,
    { attributes: true, attributeFilter: ["collapsed"] },
    () => BookmarkingUI.toolbar.collapsed != visible
  );
  let toolbarItemsVisibilityUpdated = visible
    ? BrowserTestUtils.waitForEvent(
        BookmarkingUI.toolbar,
        "BookmarksToolbarVisibilityUpdated"
      )
    : null;
  triggerFn();
  await collapsedState;
  is(
    BookmarkingUI.toolbar.getAttribute("collapsed"),
    (!visible).toString(),
    `${msg}; collapsed attribute state`
  );
  if (visible) {
    info(`${msg}; waiting for toolbar items to become visible`);
    await toolbarItemsVisibilityUpdated;
    isnot(
      BookmarkingUI.toolbar.getBoundingClientRect().height,
      0,
      `${msg}; should have a height`
    );
  } else {
    is(
      BookmarkingUI.toolbar.getBoundingClientRect().height,
      0,
      `${msg}; should have zero height`
    );
  }
}

add_task(async function () {
  registerCleanupFunction(() => {
    setToolbarVisibility(
      BookmarkingUI.toolbar,
      gBookmarksToolbarVisibility,
      false,
      false
    );
  });

  await expectBmToolbarVisibilityChange(
    () => setToolbarVisibility(BookmarkingUI.toolbar, false, false, false),
    false,
    "bookmarks toolbar is hidden initially"
  );

  let pageURL = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  );
  pageURL = `${pageURL}file_observe_height_changes.html`;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageURL);

  await expectBmToolbarVisibilityChange(
    () => setToolbarVisibility(BookmarkingUI.toolbar, true, false, false),
    true,
    "bookmarks toolbar is visible after explicitly showing it for tab with content loaded"
  );
  await expectHeightChanges(
    tab,
    1,
    "content area height changes when showing the toolbar without the animation"
  );

  await expectBmToolbarVisibilityChange(
    () => setToolbarVisibility(BookmarkingUI.toolbar, "newtab", false, false),
    false,
    "bookmarks toolbar is hidden for non-new tab after setting it to only show for new tabs"
  );
  await expectHeightChanges(
    tab,
    1,
    "content area height changes when hiding the toolbar without the animation"
  );

  info("Opening a new tab, making the previous tab non-selected");
  await expectBmToolbarVisibilityChange(
    () => {
      BrowserOpenTab();
      ok(
        !tab.selected,
        "non-new tab is in the background (not the selected tab)"
      );
    },
    true,
    "bookmarks toolbar is visible for new tab after setting it to only show for new tabs"
  );
  await expectHeightChanges(
    tab,
    0,
    "no additional content area height change in background tab when showing the bookmarks toolbar in new tab"
  );

  gBrowser.removeCurrentTab();
  gBrowser.removeTab(tab);
});
