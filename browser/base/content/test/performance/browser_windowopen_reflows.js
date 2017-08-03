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
  {
    stack: [
      "select@chrome://global/content/bindings/textbox.xml",
      "focusAndSelectUrlBar@chrome://browser/content/browser.js",
      "_delayedStartup@chrome://browser/content/browser.js",
    ],
  },
];

if (Services.appinfo.OS == "Linux") {
  if (gMultiProcessBrowser) {
    EXPECTED_REFLOWS.push({
      stack: [
        "handleEvent@chrome://browser/content/tabbrowser.xml",
      ],
    });
  } else {
    EXPECTED_REFLOWS.push({
      stack: [
        "handleEvent@chrome://browser/content/tabbrowser.xml",
        "inferFromText@chrome://browser/content/browser.js",
        "handleEvent@chrome://browser/content/browser.js",
      ],
    });
  }
}

if (Services.appinfo.OS == "Darwin") {
  EXPECTED_REFLOWS.push({
    stack: [
      "handleEvent@chrome://browser/content/tabbrowser.xml",
      "inferFromText@chrome://browser/content/browser.js",
      "handleEvent@chrome://browser/content/browser.js",
    ],
  });
}

if (Services.appinfo.OS == "WINNT") {
  EXPECTED_REFLOWS.push(
    {
      stack: [
        "verticalMargins@chrome://browser/content/browser-tabsintitlebar.js",
        "_update@chrome://browser/content/browser-tabsintitlebar.js",
        "init@chrome://browser/content/browser-tabsintitlebar.js",
        "handleEvent@chrome://browser/content/tabbrowser.xml",
      ],
      times: 2, // This number should only ever go down - never up.
    },

    {
      stack: [
        "handleEvent@chrome://browser/content/tabbrowser.xml",
        "inferFromText@chrome://browser/content/browser.js",
        "handleEvent@chrome://browser/content/browser.js",
      ],
    },

    {
      stack: [
        "handleEvent@chrome://browser/content/tabbrowser.xml",
      ],
    }
  );
}

if (Services.appinfo.OS == "WINNT" || Services.appinfo.OS == "Darwin") {
  EXPECTED_REFLOWS.push(
    {
      stack: [
        "rect@chrome://browser/content/browser-tabsintitlebar.js",
        "_update@chrome://browser/content/browser-tabsintitlebar.js",
        "init@chrome://browser/content/browser-tabsintitlebar.js",
        "handleEvent@chrome://browser/content/tabbrowser.xml",
      ],
      times: 4, // This number should only ever go down - never up.
    },

    {
      stack: [
        "verticalMargins@chrome://browser/content/browser-tabsintitlebar.js",
        "_update@chrome://browser/content/browser-tabsintitlebar.js",
        "init@chrome://browser/content/browser-tabsintitlebar.js",
        "handleEvent@chrome://browser/content/tabbrowser.xml",
      ],
      times: 2, // This number should only ever go down - never up.
    }
  );
}

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening new windows.
 */
add_task(async function() {
  const IS_WIN8 = (navigator.userAgent.indexOf("Windows NT 6.2") != -1);
  if (IS_WIN8) {
    ok(true, "Skipping this test because of perma-failures on Windows 8 x64 (bug 1381521)");
    return;
  }

  // Flushing all caches helps to ensure that we get consistent
  // behaviour when opening a new window, even if windows have been
  // opened in previous tests.
  Services.obs.notifyObservers(null, "startupcache-invalidate");
  Services.obs.notifyObservers(null, "chrome-flush-skin-caches");
  Services.obs.notifyObservers(null, "chrome-flush-caches");

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

