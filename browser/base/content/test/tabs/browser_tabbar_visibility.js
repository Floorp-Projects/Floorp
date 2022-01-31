/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);

add_task(async function testWindowOpen() {
  const DUMMY_PAGE = "browser/base/content/test/tabs/dummy_page.html";
  const TEST_URL = "http://example.com/browser/" + DUMMY_PAGE;
  const TEST_URL_CHROME = "chrome://mochitests/content/browser/" + DUMMY_PAGE;

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URL_CHROME);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  PromiseTestUtils.expectUncaughtRejection(/editor is null/); // bug 1752901
  let newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: TEST_URL,
  });
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [TEST_URL], url => {
    content.eval(`window.open("${url}", "_blank", "menubar=0")`);
  });
  let win = await newWindowPromise;

  is(win.toolbar.visible, false, "toolbar should be hidden");
  let toolbar = win.document.getElementById("TabsToolbar");
  is(toolbar.collapsed, true, "tabbar should be collapsed");

  await BrowserTestUtils.closeWindow(win);

  PromiseTestUtils.expectUncaughtRejection(/editor is null/); // bug 1752901
  newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: TEST_URL,
  });
  SessionStore.undoCloseWindow(0);
  win = await newWindowPromise;

  is(win.toolbar.visible, false, "toolbar should be hidden");
  toolbar = win.document.getElementById("TabsToolbar");
  is(toolbar.collapsed, true, "tabbar should be collapsed");

  await BrowserTestUtils.closeWindow(win);
});
