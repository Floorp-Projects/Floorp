/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(10);

const TEST_PAGE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_cancel_content_js.html";
const NEXT_PAGE = "https://example.org/";
const JS_URI = "javascript:void(document.title = 'foo')";

function sleep(ms) {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function test_navigation(nextPage, cancelContentJSPref, shouldCancel) {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.cancel_content_js_when_navigating", cancelContentJSPref]],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PAGE,
  });

  const loopEnded = ContentTask.spawn(tab.linkedBrowser, [], async function() {
    await new Promise(resolve => {
      content.addEventListener("LongLoopEnded", resolve, {
        once: true,
      });
    });
  });

  // Wait for the test page's long-running JS loop to start; it happens ~500ms
  // after load.
  await sleep(1000);

  info(
    `navigating to ${nextPage} with cancel content JS ${
      cancelContentJSPref ? "enabled" : "disabled"
    }`
  );
  const nextPageLoaded = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "DOMTitleChanged"
  );
  BrowserTestUtils.loadURI(gBrowser, nextPage);

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

add_task(async () => test_navigation(NEXT_PAGE, true, true));
add_task(async () => test_navigation(NEXT_PAGE, false, false));
add_task(async () => test_navigation(JS_URI, true, false));
add_task(async () => test_navigation(JS_URI, false, false));
