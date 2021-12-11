add_task(async function newtabPreloaded() {
  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });

  const { contentDocument: doc, contentWindow } = gBrowser;
  function dispatchMenuItemCommand(menuItem) {
    const cmdEvent = doc.createEvent("xulcommandevent");
    cmdEvent.initCommandEvent(
      "command",
      true,
      true,
      contentWindow,
      0,
      false,
      false,
      false,
      false,
      0,
      null,
      0
    );
    menuItem.dispatchEvent(cmdEvent);
  }

  const menuHome = doc.querySelector(`#newTabMode menuitem[value="0"]`);
  const menuBlank = doc.querySelector(`#newTabMode menuitem[value="1"]`);
  ok(menuHome.selected, "The first item, Home (default), is selected.");
  ok(NewTabPagePreloading.enabled, "Default Home allows preloading.");

  dispatchMenuItemCommand(menuBlank);
  ok(menuBlank.selected, "The second item, Blank, is selected.");
  ok(!NewTabPagePreloading.enabled, "Non-Home prevents preloading.");

  dispatchMenuItemCommand(menuHome);
  ok(menuHome.selected, "The first item, Home, is selected again.");
  ok(NewTabPagePreloading.enabled, "Default Home allows preloading again.");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
