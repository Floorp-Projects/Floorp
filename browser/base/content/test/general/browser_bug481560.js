add_task(async function testTabCloseShortcut() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(win);

  function onTabClose() {
    ok(false, "shouldn't have gotten the TabClose event for the last tab");
  }
  var tab = win.gBrowser.selectedTab;
  tab.addEventListener("TabClose", onTabClose);

  EventUtils.synthesizeKey("w", { accelKey: true }, win);

  ok(win.closed, "accel+w closed the window immediately");

  tab.removeEventListener("TabClose", onTabClose);
});
