add_task(async function() {
  var win = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");
  await SimpleTest.promiseFocus(win);

  let tab = win.gBrowser.tabContainer.firstChild;
  await promiseTabLoadEvent(tab, getRootDirectory(gTestPath) + "test_bug462673.html");

  is(win.gBrowser.browsers.length, 2, "test_bug462673.html has opened a second tab");
  is(win.gBrowser.selectedTab, tab.nextSibling, "dependent tab is selected");
  win.gBrowser.removeTab(tab);

  // Closing a tab will also close its parent chrome window, but async
  await BrowserTestUtils.domWindowClosed(win);
});

add_task(async function() {
  var win = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");
  await SimpleTest.promiseFocus(win);

  let tab = win.gBrowser.tabContainer.firstChild;
  await promiseTabLoadEvent(tab, getRootDirectory(gTestPath) + "test_bug462673.html");

  var newTab = win.gBrowser.addTab();
  var newBrowser = newTab.linkedBrowser;
  win.gBrowser.removeTab(tab);
  ok(!win.closed, "Window stays open");
  if (!win.closed) {
    is(win.gBrowser.tabContainer.childElementCount, 1, "Window has one tab");
    is(win.gBrowser.browsers.length, 1, "Window has one browser");
    is(win.gBrowser.selectedTab, newTab, "Remaining tab is selected");
    is(win.gBrowser.selectedBrowser, newBrowser, "Browser for remaining tab is selected");
    is(win.gBrowser.tabbox.selectedPanel, newBrowser.parentNode.parentNode.parentNode.parentNode, "Panel for remaining tab is selected");
  }

  await promiseWindowClosed(win);
});
