/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function testFeatures(win) {
  is(win.toolbar.visible, false, "toolbar should be hidden");
  let toolbar = win.document.getElementById("TabsToolbar");
  is(toolbar.collapsed, true, "tabbar should be collapsed");
  let chromeFlags = win.docShell.treeOwner
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIAppWindow).chromeFlags;
  is(
    chromeFlags & Ci.nsIWebBrowserChrome.CHROME_WINDOW_RESIZE,
    0,
    "window shouls not be resizable"
  );
}

add_task(async function testWindowOpen() {
  const DUMMY_PAGE = "browser/base/content/test/tabs/dummy_page.html";
  const TEST_URL = "http://example.com/browser/" + DUMMY_PAGE;
  const TEST_URL_CHROME = "chrome://mochitests/content/browser/" + DUMMY_PAGE;

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URL_CHROME);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: TEST_URL,
  });
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [TEST_URL], url => {
    content.eval(`window.open("${url}", "_blank", "menubar=0")`);
  });
  let win = await newWindowPromise;

  testFeatures(win);

  await BrowserTestUtils.closeWindow(win);

  newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: TEST_URL,
  });
  SessionStore.undoCloseWindow(0);
  win = await newWindowPromise;

  testFeatures(win);

  await BrowserTestUtils.closeWindow(win);
});
