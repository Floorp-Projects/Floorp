add_task(async function test() {
  // Disable tab animations
  gReduceMotionOverride = true;

  // Open Bookmarks Toolbar
  let bookmarksToolbar = document.getElementById("PersonalToolbar");
  setToolbarVisibility(bookmarksToolbar, true);
  ok(!bookmarksToolbar.collapsed, "bookmarksToolbar should be visible now");

  let tab1 = await addTab("http://mochi.test:8888/1");
  let tab2 = await addTab("http://mochi.test:8888/2");
  let tab3 = await addTab("http://mochi.test:8888/3");
  let tab4 = await addTab("http://mochi.test:8888/4");
  let tab5 = await addTab("http://mochi.test:8888/5");

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await triggerClickOn(tab1, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  ok(!tab5.multiselected, "Tab5 is not multiselected");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

  // Use getElementsByClassName so the list is live and will update as items change.
  let currentBookmarks = bookmarksToolbar.getElementsByClassName(
    "bookmark-item"
  );
  let startBookmarksLength = currentBookmarks.length;

  let lastBookmark = currentBookmarks[currentBookmarks.length - 1];
  await EventUtils.synthesizePlainDragAndDrop({
    srcElement: tab1,
    destElement: lastBookmark,
  });
  await TestUtils.waitForCondition(
    () => currentBookmarks.length == startBookmarksLength + 2,
    "waiting for 2 bookmarks"
  );
  is(
    currentBookmarks.length,
    startBookmarksLength + 2,
    "Bookmark count should have increased by 2"
  );

  // Drag non-selection to the bookmarks toolbar
  startBookmarksLength = currentBookmarks.length;
  await EventUtils.synthesizePlainDragAndDrop({
    srcElement: tab3,
    destElement: lastBookmark,
  });
  await TestUtils.waitForCondition(
    () => currentBookmarks.length == startBookmarksLength + 1,
    "waiting for 1 bookmark"
  );
  is(
    currentBookmarks.length,
    startBookmarksLength + 1,
    "Bookmark count should have increased by 1"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
  BrowserTestUtils.removeTab(tab5);
});
