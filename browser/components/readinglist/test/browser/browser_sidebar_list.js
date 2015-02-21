/**
 * This tests the basic functionality of the sidebar to list items.
 */

add_task(function*() {
  registerCleanupFunction(function*() {
    ReadingListUI.hideSidebar();
    yield RLUtils.cleanup();
  });

  RLUtils.enabled = true;

  yield ReadingListUI.showSidebar();
  let RLSidebar = RLSidebarUtils.RLSidebar;
  let sidebarDoc = SidebarUI.browser.contentDocument;
  Assert.equal(RLSidebar.numItems, 0, "Should start with no items");
  Assert.equal(RLSidebar.activeItem, null, "Should start with no active item");
  Assert.equal(RLSidebar.activeItem, null, "Should start with no selected item");

  info("Adding first item");
  yield RLUtils.addItem({
    id: "c3502a49-bcef-4a94-b222-d4834463de33",
    url: "http://example.com/article1",
    title: "Article 1",
  });
  RLSidebarUtils.expectNumItems(1);

  info("Adding more items");
  yield RLUtils.addItem([{
    id: "e054f5b7-1f4f-463f-bb96-d64c02448c31",
    url: "http://example.com/article2",
    title: "Article 2",
  }, {
    id: "4207230b-2364-4e97-9587-01312b0ce4e6",
    url: "http://example.com/article3",
    title: "Article 3",
  }]);
  RLSidebarUtils.expectNumItems(3);

  info("Closing sidebar");
  ReadingListUI.hideSidebar();

  info("Adding another item");
  yield RLUtils.addItem({
    id: "dae0e855-607e-4df3-b27f-73a5e35c94fe",
    url: "http://example.com/article4",
    title: "Article 4",
  });

  info("Re-eopning sidebar");
  yield ReadingListUI.showSidebar();
  RLSidebarUtils.expectNumItems(4);
});
