/**
 * This tests the basic functionality of the sidebar to list items.
 */

add_task(function*() {
  registerCleanupFunction(function*() {
    ReadingListUI.hideSidebar();
    yield RLUtils.cleanup();
  });

  RLUtils.enabled = true;

  yield RLSidebarUtils.showSidebar();
  let RLSidebar = RLSidebarUtils.RLSidebar;
  let sidebarDoc = SidebarUI.browser.contentDocument;
  Assert.equal(RLSidebar.numItems, 0, "Should start with no items");
  Assert.equal(RLSidebar.activeItem, null, "Should start with no active item");
  Assert.equal(RLSidebar.activeItem, null, "Should start with no selected item");

  info("Adding first item");
  yield RLUtils.addItem({
    url: "http://example.com/article1",
    title: "Article 1",
  });
  RLSidebarUtils.expectNumItems(1);

  info("Adding more items");
  yield RLUtils.addItem([{
    url: "http://example.com/article2",
    title: "Article 2",
  }, {
    url: "http://example.com/article3",
    title: "Article 3",
  }]);
  RLSidebarUtils.expectNumItems(3);

  info("Closing sidebar");
  ReadingListUI.hideSidebar();

  info("Adding another item");
  yield RLUtils.addItem({
    url: "http://example.com/article4",
    title: "Article 4",
  });

  info("Re-opening sidebar");
  yield RLSidebarUtils.showSidebar();
  RLSidebarUtils.expectNumItems(4);
});
