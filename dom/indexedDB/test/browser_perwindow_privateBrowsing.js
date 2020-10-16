/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL =
  "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsPrompt.html";
const notificationID = "indexedDB-permissions-prompt";

async function doTest(browser) {
  info("creating tab");
  browser.selectedTab = BrowserTestUtils.addTab(browser);

  info("loading test page: " + testPageURL);
  BrowserTestUtils.loadURI(browser.selectedBrowser, testPageURL);

  await waitForMessage(true, browser);
  browser.removeCurrentTab();
}

add_task(async function test1() {
  // Avoids the actual prompt
  setPermission(testPageURL, "indexedDB");

  await doTest(gBrowser);
});

add_task(async function test2() {
  info("creating private window");
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  // Avoids the actual prompt
  setPermission(testPageURL, "indexedDB", { privateBrowsingId: 1 });

  await doTest(win.gBrowser);

  await BrowserTestUtils.closeWindow(win);
  win = null;
});
