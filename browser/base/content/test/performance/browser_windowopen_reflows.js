/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */
const EXPECTED_REFLOWS = [
  [
    "select@chrome://global/content/bindings/textbox.xml",
    "focusAndSelectUrlBar@chrome://browser/content/browser.js",
    "_delayedStartup@chrome://browser/content/browser.js",
  ],
];

if (Services.appinfo.OS == "Darwin") {
  // TabsInTitlebar._update causes a reflow on OS X trying to do calculations
  // since layout info is already dirty. This doesn't seem to happen before
  // MozAfterPaint on Linux.
  EXPECTED_REFLOWS.push(
    [
      "rect@chrome://browser/content/browser-tabsintitlebar.js",
      "_update@chrome://browser/content/browser-tabsintitlebar.js",
      "updateAppearance@chrome://browser/content/browser-tabsintitlebar.js",
      "handleEvent@chrome://browser/content/tabbrowser.xml",
    ],
  );
}

if (Services.appinfo.OS == "WINNT" || Services.appinfo.OS == "Darwin") {
  EXPECTED_REFLOWS.push(
    [
      "handleEvent@chrome://browser/content/tabbrowser.xml",
      "inferFromText@chrome://browser/content/browser.js",
      "handleEvent@chrome://browser/content/browser.js",
    ],
  );
}

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening new windows.
 */
add_task(async function() {
  let win = OpenBrowserWindow();

  await withReflowObserver(async function() {
    let resizeEvent = BrowserTestUtils.waitForEvent(win, "resize");
    let delayedStartup =
      TestUtils.topicObserved("browser-delayed-startup-finished",
                              subject => subject == win);
    await resizeEvent;
    await delayedStartup;
  }, EXPECTED_REFLOWS, win);

  await BrowserTestUtils.closeWindow(win);
});

