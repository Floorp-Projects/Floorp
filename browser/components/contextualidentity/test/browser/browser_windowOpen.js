"use strict";

// Here we want to test that a new opened window shows the same UI of the
// parent one if this has been loaded from a particular container.

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "contextualidentity/test/browser/empty_file.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
    ["browser.link.open_newwindow", 2],
  ]});
});


add_task(async function test() {
  info("Creating a tab with UCI = 1...");
  let tab = BrowserTestUtils.addTab(gBrowser, BASE_URI, {userContextId: 1});
  is(tab.getAttribute("usercontextid"), 1, "New tab has UCI equal 1");

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Opening a new window from this tab...");
  ContentTask.spawn(browser, BASE_URI, function(url) {
    content.window.newWindow = content.window.open(url, "_blank");
  });

  let newWin = await BrowserTestUtils.waitForNewWindow();
  let newTab = newWin.gBrowser.selectedTab;

  await BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
  is(newTab.getAttribute("usercontextid"), 1, "New tab has UCI equal 1");

  info("Closing the new window and tab...");
  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.removeTab(tab);
});
