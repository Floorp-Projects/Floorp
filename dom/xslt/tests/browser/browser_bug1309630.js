"use strict";

const BASE = "https://example.com/browser/dom/xslt/tests/browser";
const SERVER_SCRIPT = `${BASE}/bug1309630.sjs`;

function resetCounter() {
  return fetch(`${SERVER_SCRIPT}?reset_counter`);
}
function recordCounter() {
  return fetch(`${SERVER_SCRIPT}?record_counter`);
}
// Returns a promise that resolves to true if the counter in
// bug1309630.sjs changed by more than 'value' since last calling
// recordCounter(), or false if it doesn't and we time out.
function waitForCounterChangeAbove(value) {
  return TestUtils.waitForCondition(() =>
    fetch(`${SERVER_SCRIPT}?get_counter_change`).then(response =>
      response.ok
        ? response.text().then(str => Number(str) > value)
        : Promise.reject()
    )
  ).then(
    () => true,
    () => false
  );
}

add_task(async function test_eternal_xslt() {
  await resetCounter();
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: SERVER_SCRIPT, waitForLoad: false },
    async function (browser) {
      info("Waiting for XSLT to keep loading");

      ok(
        await waitForCounterChangeAbove(1),
        "We should receive at least a request from the document function call."
      );

      info("Navigating to about:blank");
      BrowserTestUtils.startLoadingURIString(browser, "about:blank");
      await BrowserTestUtils.browserLoaded(browser);

      info("Check to see if XSLT stops loading");
      await recordCounter();
      ok(
        !(await waitForCounterChangeAbove(0)),
        "We shouldn't receive more requests to the XSLT file within the timeout period."
      );
    }
  );

  await resetCounter();
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `${BASE}/file_bug1309630.html` },
    async function (browser) {
      ok(
        await waitForCounterChangeAbove(1),
        "We should receive at least a request from the document function call."
      );

      info("Navigating to about:blank");
      BrowserTestUtils.startLoadingURIString(browser, "about:blank");
      await BrowserTestUtils.browserLoaded(browser);

      info("Check to see if XSLT stops loading");
      await recordCounter();
      ok(
        !(await waitForCounterChangeAbove(0)),
        "We shouldn't receive more requests to the XSLT file within the timeout period."
      );
    }
  );
});
