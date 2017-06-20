var win;
var feedItem;
var container;

SimpleTest.requestCompleteLog();

add_task(async function setup() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  info("Preferences page opened on the paneGeneral pane.");

  registerCleanupFunction(() => {
    gBrowser.removeCurrentTab();
  });
});

add_task(async function getFeedItem() {
  win = gBrowser.selectedBrowser.contentWindow;

  container = win.document.getElementById("handlersView");
  feedItem = container.querySelector("richlistitem[type='application/vnd.mozilla.maybe.feed']");
  Assert.ok(feedItem, "feedItem is present in handlersView.");
})

add_task(async function selectInternalOptionForFeed() {
  // Select the item.
  feedItem.scrollIntoView();
  container.selectItem(feedItem);
  Assert.ok(feedItem.selected, "Should be able to select our item.");

  // Wait for the menu.
  let list = await waitForCondition(() =>
    win.document.getAnonymousElementByAttribute(feedItem, "class", "actionsMenu"));
  info("Got list after item was selected");

  // Find the "Add Live bookmarks option".
  let chooseItems = list.getElementsByAttribute("action", Ci.nsIHandlerInfo.handleInternally);
  Assert.equal(chooseItems.length, 1, "Should only be one action to handle internally");

  // Select the option.
  let cmdEvent = win.document.createEvent("xulcommandevent");
  cmdEvent.initCommandEvent("command", true, true, win, 0, false, false, false, false, null);
  chooseItems[0].dispatchEvent(cmdEvent);

  // Check that we display the correct result.
  list = await waitForCondition(() =>
    win.document.getAnonymousElementByAttribute(feedItem, "class", "actionsMenu"));
  info("Got list after item was selected");
  Assert.ok(list.selectedItem, "Should have a selected item.");
  Assert.equal(list.selectedItem.getAttribute("action"),
               Ci.nsIHandlerInfo.handleInternally,
               "Newly selected item should be the expected one.");
});

// This builds on the previous selectInternalOptionForFeed task.
add_task(async function reselectInternalOptionForFeed() {
  // Now select a different option in the list - use the pdf item as that doesn't
  // need to load any favicons.
  let anotherItem = container.querySelector("richlistitem[type='application/pdf']");

  container.selectItem(anotherItem);

  // Wait for the menu so that we don't hit race conditions.
  await waitForCondition(() =>
    win.document.getAnonymousElementByAttribute(anotherItem, "class", "actionsMenu"));
  info("Got list after item was selected");

  // Now select the feed item again, and check what it is displaying.
  container.selectItem(feedItem);

  let list = await waitForCondition(() =>
    win.document.getAnonymousElementByAttribute(feedItem, "class", "actionsMenu"));
  info("Got list after item was selected");

  Assert.ok(list.selectedItem,
            "Should have a selected item");
  Assert.equal(list.selectedItem.getAttribute("action"),
               Ci.nsIHandlerInfo.handleInternally,
               "Selected item should still be the same as the previously selected item.");
});
