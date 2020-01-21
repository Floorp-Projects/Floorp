/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const TEST_HOST = "example.org";
const TEST_DOMAIN = "https://" + TEST_HOST;
const TEST_PATH = "/dom/reporting/tests/";
const TEST_TOP_PAGE = TEST_DOMAIN + "/browser" + TEST_PATH + "empty.html";
const TEST_SJS = TEST_DOMAIN + "/tests" + TEST_PATH + "delivering.sjs";

function crash_content(browser) {
  browser.messageManager.loadFrameScript(
    "data:,Components.classes['@mozilla.org/xpcom/debug;1'].getService(Components.interfaces.nsIDebug2).rustPanic('OH NO');",
    false
  );
}

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before the test"
  );

  await storeReportingHeader(browser, TEST_SJS);
  ok(
    ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "We have data for the origin"
  );

  crash_content(browser);

  const reports = await checkReport(TEST_SJS);
  is(reports.length, 1, "We have 1 report");
  const report = reports[0];
  is(report.contentType, "application/reports+json", "Correct mime-type");
  is(report.origin, "https://example.org", "Origin correctly set");
  is(
    report.url,
    "https://example.org/tests/dom/reporting/tests/delivering.sjs",
    "URL is correctly set"
  );
  ok(!!report.body, "We have a report.body");
  ok(report.body.age > 0, "Age is correctly set");
  is(report.body.url, TEST_TOP_PAGE, "URL is correctly set");
  is(report.body.user_agent, navigator.userAgent, "User-agent matches");
  is(report.body.type, "crash", "Type is fine.");

  BrowserTestUtils.removeTab(tab);
});
