/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FRAME_SCRIPT = getRootDirectory(gTestPath) + "content-reflows.js";
const ADDITIONAL_WAIT_MS = 2000;

/*
 * Ensure that loading about:newtab doesn't cause uninterruptible reflows.
 */
add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    return gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {animate: false});
  }, false);

  let browser = gBrowser.selectedBrowser;
  let mm = browser.messageManager;
  mm.loadFrameScript(FRAME_SCRIPT, true);
  mm.addMessageListener("newtab-reflow", ({data: stack}) => {
    ok(false, `unexpected uninterruptible reflow ${stack}`);
  });

  let browserLoadedPromise = BrowserTestUtils.waitForEvent(browser, "load", true);
  browser.loadURI("about:newtab");
  await browserLoadedPromise;

  // Wait some more to catch sync reflows after the page has loaded.
  await new Promise(resolve => {
    setTimeout(resolve, ADDITIONAL_WAIT_MS);
  });

  // Clean up.
  gBrowser.removeCurrentTab({animate: false});

  ok(true, "Each test requires at least one pass, fail or todo so here is a pass.");
});
