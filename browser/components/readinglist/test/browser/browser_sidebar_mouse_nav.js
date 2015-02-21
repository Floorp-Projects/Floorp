/**
 * Test mouse navigation for selecting items in the sidebar.
 */


function mouseInteraction(mouseEvent, responseEvent, itemNode) {
  let eventPromise = BrowserUITestUtils.waitForEvent(RLSidebarUtils.list, responseEvent);
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
    id: "00bd24c7-3629-40b0-acde-37aa81768735",
    url: "http://example.com/article1",
    title: "Article 1",
  }, {
    id: "28bf7f19-cf94-4ceb-876a-ac1878342e0d",
    url: "http://example.com/article2",
    title: "Article 2",
  }, {
    id: "7e5064ea-f45d-4fc7-8d8c-c067b7781e78",
    url: "http://example.com/article3",
    title: "Article 3",
  }, {
    id: "8e72a472-8db8-4904-ba39-9672f029e2d0",
    url: "http://example.com/article4",
    title: "Article 4",
  }, {
    id: "8d332744-37bc-4a1a-a26b-e9953b9f7d91",
    url: "http://example.com/article5",
    title: "Article 5",
  }];
  info("Adding initial mock data");
  yield RLUtils.addItem(itemData);

  info("Opening sidebar");
  yield ReadingListUI.showSidebar();
  RLSidebarUtils.expectNumItems(5);
  RLSidebarUtils.expectSelectedId(null);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse move over item 1");
  yield mouseInteraction("mousemove", "SelectedItemChanged", RLSidebarUtils.list.children[0]);
  RLSidebarUtils.expectSelectedId(itemData[0].id);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse move over item 2");
  yield mouseInteraction("mousemove", "SelectedItemChanged", RLSidebarUtils.list.children[1]);
  RLSidebarUtils.expectSelectedId(itemData[1].id);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse move over item 5");
  yield mouseInteraction("mousemove", "SelectedItemChanged", RLSidebarUtils.list.children[4]);
  RLSidebarUtils.expectSelectedId(itemData[4].id);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse move over item 1 again");
  yield mouseInteraction("mousemove", "SelectedItemChanged", RLSidebarUtils.list.children[0]);
  RLSidebarUtils.expectSelectedId(itemData[0].id);
  RLSidebarUtils.expectActiveId(null);

  info("Mouse click on item 1");
  yield mouseInteraction("click", "ActiveItemChanged", RLSidebarUtils.list.children[0]);
  RLSidebarUtils.expectSelectedId(itemData[0].id);
  RLSidebarUtils.expectActiveId(itemData[0].id);

  info("Mouse click on item 3");
  yield mouseInteraction("click", "ActiveItemChanged", RLSidebarUtils.list.children[2]);
  RLSidebarUtils.expectSelectedId(itemData[2].id);
  RLSidebarUtils.expectActiveId(itemData[2].id);
});
