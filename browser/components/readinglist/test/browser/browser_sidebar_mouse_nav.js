/**
 * Test mouse navigation for selecting items in the sidebar.
 */


function mouseInteraction(mouseEvent, responseEvent, itemNode) {
  let eventPromise = BrowserTestUtils.waitForEvent(RLSidebarUtils.list, responseEvent);
  let details = {};
  if (mouseEvent != "click") {
    details.type = mouseEvent;
  }

  EventUtils.synthesizeMouseAtCenter(itemNode, details, itemNode.ownerDocument.defaultView);
  return eventPromise;
}

add_task(function*() {
  registerCleanupFunction(function*() {
    ReadingListUI.hideSidebar();
    yield RLUtils.cleanup();
  });

  RLUtils.enabled = true;

  let itemData = [{
    url: "http://example.com/article1",
    title: "Article 1",
  }, {
    url: "http://example.com/article2",
    title: "Article 2",
  }, {
    url: "http://example.com/article3",
    title: "Article 3",
  }, {
    url: "http://example.com/article4",
    title: "Article 4",
  }, {
    url: "http://example.com/article5",
    title: "Article 5",
  }];
  info("Adding initial mock data");
  yield RLUtils.addItem(itemData);

  info("Fetching items");
  let items = yield ReadingList.iterator({ sort: "url" }).items(itemData.length);

  info("Opening sidebar");
  yield RLSidebarUtils.showSidebar();
  RLSidebarUtils.expectNumItems(5);
  RLSidebarUtils.expectSelectedId(null);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse move over item 1");
  yield mouseInteraction("mousemove", "SelectedItemChanged", RLSidebarUtils.list.children[0]);
  RLSidebarUtils.expectSelectedId(items[0].id);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse move over item 2");
  yield mouseInteraction("mousemove", "SelectedItemChanged", RLSidebarUtils.list.children[1]);
  RLSidebarUtils.expectSelectedId(items[1].id);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse move over item 5");
  yield mouseInteraction("mousemove", "SelectedItemChanged", RLSidebarUtils.list.children[4]);
  RLSidebarUtils.expectSelectedId(items[4].id);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse move over item 1 again");
  yield mouseInteraction("mousemove", "SelectedItemChanged", RLSidebarUtils.list.children[0]);
  RLSidebarUtils.expectSelectedId(items[0].id);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse click on item 1");
  yield mouseInteraction("click", "ActiveItemChanged", RLSidebarUtils.list.children[0]);
  RLSidebarUtils.expectSelectedId(items[0].id);
  RLSidebarUtils.expectActiveId(items[0].id);

  info("Mouse click on item 3");
  yield mouseInteraction("click", "ActiveItemChanged", RLSidebarUtils.list.children[2]);
  RLSidebarUtils.expectSelectedId(items[2].id);
  RLSidebarUtils.expectActiveId(items[2].id);
});
