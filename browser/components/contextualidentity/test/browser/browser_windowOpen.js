"use strict";

// Here we want to test that a new opened window shows the same UI of the
// parent one if this has been loaded from a particular container.

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "contextualidentity/test/browser/empty_file.html";

add_task(function* setup() {
  yield new Promise((resolve) => {
    SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true],
      ["browser.link.open_newwindow", 2],
    ]}, resolve);
  });
});


add_task(function* test() {
  info("Creating a tab with UCI = 1...");
  let tab = gBrowser.addTab(BASE_URI, {userContextId: 1});
  is(tab.getAttribute('usercontextid'), 1, "New tab has UCI equal 1");

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);

  info("Opening a new window from this tab...");
  ContentTask.spawn(browser, BASE_URI, function(url) {
    content.window.newWindow = content.window.open(url, "_blank");
  });

  let newWin = yield BrowserTestUtils.waitForNewWindow();
  let newTab = newWin.gBrowser.selectedTab;

  yield BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
  is(newTab.getAttribute('usercontextid'), 1, "New tab has UCI equal 1");

  info("Closing the new window and tab...");
  yield BrowserTestUtils.closeWindow(newWin);
  yield BrowserTestUtils.removeTab(tab);
});
