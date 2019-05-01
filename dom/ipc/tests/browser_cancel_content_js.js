"use strict";

const TEST_PAGE = "http://mochi.test:8888/browser/dom/ipc/tests/file_cancel_content_js.html";
const NEXT_PAGE = "https://example.org/";

function sleep(ms) {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function test_navigation(cancelContentJS) {
  await SpecialPowers.pushPrefEnv({
    "set": [
      ["dom.ipc.cancel_content_js_when_navigating", cancelContentJS],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser, opening: TEST_PAGE,
  });

  await sleep(1000);
  BrowserTestUtils.loadURI(gBrowser, NEXT_PAGE);
  const nextPageLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser,
                                                        false, NEXT_PAGE);
  const result = await Promise.race([
    nextPageLoaded,
    sleep(1000).then(() => "timeout"),
  ]);

  const timedOut = result === "timeout";
  if (cancelContentJS) {
    ok(timedOut === false, "expected next page to be loaded");
  } else {
    ok(timedOut === true, "expected timeout");
    // Give the page time to finish its long-running JS before we close the
    // tab...
    await nextPageLoaded;
  }

  BrowserTestUtils.removeTab(tab);
}

add_task(async () => test_navigation(true));
add_task(async () => test_navigation(false));
