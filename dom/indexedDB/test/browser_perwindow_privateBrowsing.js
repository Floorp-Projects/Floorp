/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsPrompt.html";
const notificationID = "indexedDB-permissions-prompt";

add_task(async function test1() {
  // Avoids the actual prompt
  setPermission(testPageURL, "indexedDB");

  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);

  await waitForMessage(true, gBrowser);
  gBrowser.removeCurrentTab();
});

add_task(async function test2() {
  info("creating private window");
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  info("creating tab");
  win.gBrowser.selectedTab = win.gBrowser.addTab();
  win.gBrowser.selectedBrowser.loadURI(testPageURL);
  await waitForMessage("InvalidStateError", win.gBrowser);
  win.gBrowser.removeCurrentTab();
  await BrowserTestUtils.closeWindow(win);
  win = null;
});
