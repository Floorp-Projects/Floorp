/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Test for bug 1578379. */

add_task(async function test_window_open_about_blank() {
  const URL =
    "http://mochi.test:8888/browser/docshell/test/browser/file_open_about_blank.html";
  let firstTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:blank"
  );

  info("Opening about:blank using a click");
  await SpecialPowers.spawn(firstTab.linkedBrowser, [""], async function () {
    content.document.querySelector("#open").click();
  });

  info("Waiting for the second tab to be opened");
  let secondTab = await promiseTabOpened;

  info("Detaching tab");
  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  gBrowser.replaceTabWithWindow(secondTab);
  let win = await windowOpenedPromise;

  info("Asserting document is visible");
  let tab = win.gBrowser.selectedTab;
  await SpecialPowers.spawn(tab.linkedBrowser, [""], async function () {
    is(
      content.document.visibilityState,
      "visible",
      "Document should be visible"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.removeTab(firstTab);
});

add_task(async function test_detach_loading_page() {
  const URL =
    "http://mochi.test:8888/browser/docshell/test/browser/file_slow_load.sjs";
  // Open a dummy tab so that detaching the second tab works.
  let dummyTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let slowLoadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    URL,
    /* waitForLoad = */ false
  );

  info("Wait for content document to be created");
  await BrowserTestUtils.waitForCondition(async function () {
    return SpecialPowers.spawn(
      slowLoadingTab.linkedBrowser,
      [URL],
      async function (url) {
        return content.document.documentURI == url;
      }
    );
  });

  info("Detaching tab");
  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  gBrowser.replaceTabWithWindow(slowLoadingTab);
  let win = await windowOpenedPromise;

  info("Asserting document is visible");
  let tab = win.gBrowser.selectedTab;
  await SpecialPowers.spawn(tab.linkedBrowser, [""], async function () {
    is(content.document.readyState, "loading");
    is(content.document.visibilityState, "visible");
  });

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.removeTab(dummyTab);
});
