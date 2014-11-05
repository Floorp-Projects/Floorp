/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FRAME_SCRIPT = getRootDirectory(gTestPath) + "content-reflows.js";
const ADDITIONAL_WAIT_MS = 2000;

/*
 * Ensure that loading about:newtab doesn't cause uninterruptible reflows.
 */
function runTests() {
  gBrowser.selectedTab = gBrowser.addTab("about:blank", {animate: false});
  let browser = gBrowser.selectedBrowser;
  yield whenBrowserLoaded(browser);

  let mm = browser.messageManager;
  mm.loadFrameScript(FRAME_SCRIPT, true);
  mm.addMessageListener("newtab-reflow", ({data: stack}) => {
    ok(false, `unexpected uninterruptible reflow ${stack}`);
  });

  browser.loadURI("about:newtab");
  yield whenBrowserLoaded(browser);

  // Wait some more to catch sync reflows after the page has loaded.
  yield setTimeout(TestRunner.next, ADDITIONAL_WAIT_MS);

  // Clean up.
  gBrowser.removeCurrentTab({animate: false});

  ok(true, "Each test requires at least one pass, fail or todo so here is a pass.");
}

function whenBrowserLoaded(browser) {
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    executeSoon(TestRunner.next);
  }, true);
}
