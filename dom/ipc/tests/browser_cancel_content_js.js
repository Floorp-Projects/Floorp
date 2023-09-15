/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(10);

const TEST_PAGE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_cancel_content_js.html";
const NEXT_PAGE = "http://mochi.test:8888/browser/dom/ipc/tests/";
const JS_URI = "javascript:void(document.title = 'foo')";

async function test_navigation(nextPage, shouldCancel) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.max_script_run_time", 20],
      // Force a single process so that the navigation will complete in the same
      // process as the previous page which is running the long-running script.
      ["dom.ipc.processCount", 1],
      ["dom.ipc.processCount.webIsolated", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PAGE,
  });

  const loopEnded = ContentTask.spawn(tab.linkedBrowser, [], async function () {
    await new Promise(resolve => {
      content.addEventListener("LongLoopEnded", resolve, {
        once: true,
      });
    });
  });

  // Wait for the test page's long-running JS loop to start.
  await ContentTask.spawn(tab.linkedBrowser, [], function () {
    content.dispatchEvent(new content.Event("StartLongLoop"));
  });

  info(`navigating to ${nextPage}`);
  const nextPageLoaded = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "DOMTitleChanged"
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, nextPage);

  const result = await Promise.race([
    nextPageLoaded,
    loopEnded.then(() => "timeout"),
  ]);

  const timedOut = result === "timeout";
  if (shouldCancel) {
    ok(timedOut === false, "expected next page to be loaded");
  } else {
    ok(timedOut === true, "expected timeout");
  }

  BrowserTestUtils.removeTab(tab);
}

add_task(async () => test_navigation(NEXT_PAGE, true));
add_task(async () => test_navigation(JS_URI, false));
