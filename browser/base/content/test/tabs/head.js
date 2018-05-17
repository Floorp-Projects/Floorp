function updateTabContextMenu(tab) {
  let menu = document.getElementById("tabContextMenu");
  if (!tab)
    tab = gBrowser.selectedTab;
  var evt = new Event("");
  tab.dispatchEvent(evt);
  menu.openPopup(tab, "end_after", 0, 0, true, false, evt);
  is(TabContextMenu.contextTab, tab, "TabContextMenu context is the expected tab");
  menu.hidePopup();
}

function triggerClickOn(target, options) {
  let promise = BrowserTestUtils.waitForEvent(target, "click");
  if (AppConstants.platform == "macosx") {
      options = {
          metaKey: options.ctrlKey,
          shiftKey: options.shiftKey
      };
  }
  EventUtils.synthesizeMouseAtCenter(target, options);
  return promise;
}

async function addTab() {
  const tab = BrowserTestUtils.addTab(gBrowser,
      "http://mochi.test:8888/", { skipAnimation: true });
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return tab;
}
