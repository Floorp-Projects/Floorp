"use strict";

const { TelemetryArchiveTesting } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryArchiveTesting.sys.mjs"
);

const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const kTestXFrameOptionsURI = kTestPath + "file_framing_error_pages_xfo.html";
const kTestCspURI = kTestPath + "file_framing_error_pages_csp.html";
const kTestXFrameOptionsURIFrame =
  kTestPath + "file_framing_error_pages.sjs?xfo";
const kTestCspURIFrame = kTestPath + "file_framing_error_pages.sjs?csp";

const kTestExpectedPingXFO = [
  [["payload", "error_type"], "xfo"],
  [["payload", "xfo_header"], "deny"],
  [["payload", "csp_header"], ""],
  [["payload", "frame_hostname"], "example.com"],
  [["payload", "top_hostname"], "example.com"],
  [
    ["payload", "frame_uri"],
    "https://example.com/browser/dom/security/test/general/file_framing_error_pages.sjs",
  ],
  [
    ["payload", "top_uri"],
    "https://example.com/browser/dom/security/test/general/file_framing_error_pages_xfo.html",
  ],
];

const kTestExpectedPingCSP = [
  [["payload", "error_type"], "csp"],
  [["payload", "xfo_header"], ""],
  [["payload", "csp_header"], "'none'"],
  [["payload", "frame_hostname"], "example.com"],
  [["payload", "top_hostname"], "example.com"],
  [
    ["payload", "frame_uri"],
    "https://example.com/browser/dom/security/test/general/file_framing_error_pages.sjs",
  ],
  [
    ["payload", "top_uri"],
    "https://example.com/browser/dom/security/test/general/file_framing_error_pages_csp.html",
  ],
];

const TEST_CASES = [
  {
    type: "xfo",
    test_uri: kTestXFrameOptionsURI,
    frame_uri: kTestXFrameOptionsURIFrame,
    expected_ping: kTestExpectedPingXFO,
  },
  {
    type: "csp",
    test_uri: kTestCspURI,
    frame_uri: kTestCspURIFrame,
    expected_ping: kTestExpectedPingCSP,
  },
];

add_setup(async function () {
  Services.telemetry.setEventRecordingEnabled("security.ui.xfocsperror", true);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.xfocsp.errorReporting.enabled", true],
      ["security.xfocsp.errorReporting.automatic", false],
    ],
  });
});

add_task(async function testReportingCases() {
  for (const test of TEST_CASES) {
    await testReporting(test);
  }
});

async function testReporting(test) {
  // Clear telemetry event before testing.
  Services.telemetry.clearEvents();

  let telemetryChecker = new TelemetryArchiveTesting.Checker();
  await telemetryChecker.promiseInit();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let browser = tab.linkedBrowser;

  let loaded = BrowserTestUtils.browserLoaded(
    browser,
    true,
    test.frame_uri,
    true
  );
  BrowserTestUtils.startLoadingURIString(browser, test.test_uri);
  await loaded;

  let { type } = test;

  let frameBC = await SpecialPowers.spawn(browser, [], async _ => {
    const iframe = content.document.getElementById("testframe");
    return iframe.browsingContext;
  });

  await SpecialPowers.spawn(frameBC, [type], async obj => {
    // Wait until the reporting UI is visible.
    await ContentTaskUtils.waitForCondition(() => {
      let reportUI = content.document.getElementById("blockingErrorReporting");
      return ContentTaskUtils.isVisible(reportUI);
    });

    let reportCheckBox = content.document.getElementById(
      "automaticallyReportBlockingInFuture"
    );
    is(
      reportCheckBox.checked,
      false,
      "The checkbox of the reporting ui should be not checked."
    );

    // Click on the checkbox.
    await EventUtils.synthesizeMouseAtCenter(reportCheckBox, {}, content);
  });
  BrowserTestUtils.removeTab(tab);

  // Open the error page again
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  browser = tab.linkedBrowser;

  loaded = BrowserTestUtils.browserLoaded(browser, true, test.frame_uri, true);
  BrowserTestUtils.startLoadingURIString(browser, test.test_uri);
  await loaded;

  frameBC = await SpecialPowers.spawn(browser, [], async _ => {
    const iframe = content.document.getElementById("testframe");
    return iframe.browsingContext;
  });

  await SpecialPowers.spawn(frameBC, [], async _ => {
    // Wait until the reporting UI is visible.
    await ContentTaskUtils.waitForCondition(() => {
      let reportUI = content.document.getElementById("blockingErrorReporting");
      return ContentTaskUtils.isVisible(reportUI);
    });

    let reportCheckBox = content.document.getElementById(
      "automaticallyReportBlockingInFuture"
    );
    is(
      reportCheckBox.checked,
      true,
      "The checkbox of the reporting ui should be checked."
    );

    // Click on the checkbox again to disable the reporting.
    await EventUtils.synthesizeMouseAtCenter(reportCheckBox, {}, content);

    is(
      reportCheckBox.checked,
      false,
      "The checkbox of the reporting ui should be unchecked."
    );
  });
  BrowserTestUtils.removeTab(tab);

  // Open the error page again to see if the reporting is disabled.
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  browser = tab.linkedBrowser;

  loaded = BrowserTestUtils.browserLoaded(browser, true, test.frame_uri, true);
  BrowserTestUtils.startLoadingURIString(browser, test.test_uri);
  await loaded;

  frameBC = await SpecialPowers.spawn(browser, [], async _ => {
    const iframe = content.document.getElementById("testframe");
    return iframe.browsingContext;
  });

  await SpecialPowers.spawn(frameBC, [], async _ => {
    // Wait until the reporting UI is visible.
    await ContentTaskUtils.waitForCondition(() => {
      let reportUI = content.document.getElementById("blockingErrorReporting");
      return ContentTaskUtils.isVisible(reportUI);
    });

    let reportCheckBox = content.document.getElementById(
      "automaticallyReportBlockingInFuture"
    );
    is(
      reportCheckBox.checked,
      false,
      "The checkbox of the reporting ui should be unchecked."
    );
  });
  BrowserTestUtils.removeTab(tab);

  // Finally, check if the ping has been archived.
  await new Promise(resolve => {
    telemetryChecker
      .promiseFindPing("xfocsp-error-report", test.expected_ping)
      .then(
        found => {
          ok(found, "Telemetry ping submitted successfully");
          resolve();
        },
        err => {
          ok(false, "Exception finding telemetry ping: " + err);
          resolve();
        }
      );
  });
}
