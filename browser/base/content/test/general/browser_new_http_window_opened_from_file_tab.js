/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_FILE = "file_with_link_to_http.html";
const TEST_HTTP = "http://example.org/";

// Test for bug 1338375.
add_task(async function() {
  // Open file:// page.
  let dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_FILE);
  const uriString = Services.io.newFileURI(dir).spec;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uriString);
  registerCleanupFunction(async function() {
    await BrowserTestUtils.removeTab(tab);
  });
  let browser = tab.linkedBrowser;

  // Set pref to open in new window.
  Services.prefs.setIntPref("browser.link.open_newwindow", 2);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.link.open_newwindow");
  });

  // Open new http window from JavaScript in file:// page and check that we get
  // a new window with the correct page and features.
  let promiseNewWindow = BrowserTestUtils.waitForNewWindow(true, TEST_HTTP);
  await ContentTask.spawn(browser, TEST_HTTP, uri => {
    content.open(uri, "_blank");
  });
  let win = await promiseNewWindow;
  registerCleanupFunction(async function() {
    await BrowserTestUtils.closeWindow(win);
  });
  ok(win, "Check that an http window loaded when using window.open.");
  ok(win.menubar.visible,
     "Check that the menu bar on the new window is visible.");
  ok(win.toolbar.visible,
     "Check that the tool bar on the new window is visible.");

  // Open new http window from a link in file:// page and check that we get a
  // new window with the correct page and features.
  promiseNewWindow = BrowserTestUtils.waitForNewWindow(true, TEST_HTTP);
  await BrowserTestUtils.synthesizeMouseAtCenter("#linkToExample", {}, browser);
  let win2 = await promiseNewWindow;
  registerCleanupFunction(async function() {
    await BrowserTestUtils.closeWindow(win2);
  });
  ok(win2, "Check that an http window loaded when using link.");
  ok(win2.menubar.visible,
     "Check that the menu bar on the new window is visible.");
  ok(win2.toolbar.visible,
     "Check that the tool bar on the new window is visible.");
});
