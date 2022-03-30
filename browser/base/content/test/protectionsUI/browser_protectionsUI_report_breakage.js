/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";
const BENIGN_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/benignPage.html";
const COOKIE_PAGE =
  "http://not-tracking.example.com/browser/browser/base/content/test/protectionsUI/cookiePage.html";

const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";
const TP_PREF = "privacy.trackingprotection.enabled";
const CB_PREF = "network.cookie.cookieBehavior";

const PREF_REPORT_BREAKAGE_URL = "browser.contentblocking.reportBreakage.url";

let { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
let { CommonUtils } = ChromeUtils.import("resource://services-common/utils.js");
let { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(() => {
    // Clear prefs that are touched in this test again for sanity.
    Services.prefs.clearUserPref(TP_PREF);
    Services.prefs.clearUserPref(CB_PREF);
    Services.prefs.clearUserPref(FP_PREF);
    Services.prefs.clearUserPref(CM_PREF);
    Services.prefs.clearUserPref(PREF_REPORT_BREAKAGE_URL);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "urlclassifier.features.fingerprinting.blacklistHosts",
        "fingerprinting.example.com",
      ],
      [
        "urlclassifier.features.fingerprinting.annotate.blacklistHosts",
        "fingerprinting.example.com",
      ],
      ["privacy.trackingprotection.cryptomining.enabled", true],
      [
        "urlclassifier.features.cryptomining.blacklistHosts",
        "cryptomining.example.com",
      ],
      [
        "urlclassifier.features.cryptomining.annotate.blacklistHosts",
        "cryptomining.example.com",
      ],
    ],
  });
});

add_task(async function testReportBreakageCancel() {
  Services.prefs.setBoolPref(TP_PREF, true);

  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function() {
    await openProtectionsPanel();
    await TestUtils.waitForCondition(() =>
      gProtectionsHandler._protectionsPopup.hasAttribute("blocking")
    );

    let siteNotWorkingButton = document.getElementById(
      "protections-popup-tp-switch-breakage-link"
    );
    ok(
      BrowserTestUtils.is_visible(siteNotWorkingButton),
      "site not working button is visible"
    );
    let siteNotWorkingView = document.getElementById(
      "protections-popup-siteNotWorkingView"
    );
    let viewShown = BrowserTestUtils.waitForEvent(
      siteNotWorkingView,
      "ViewShown"
    );
    siteNotWorkingButton.click();
    await viewShown;

    let sendReportButton = document.getElementById(
      "protections-popup-siteNotWorkingView-sendReport"
    );
    let sendReportView = document.getElementById(
      "protections-popup-sendReportView"
    );
    viewShown = BrowserTestUtils.waitForEvent(sendReportView, "ViewShown");
    sendReportButton.click();
    await viewShown;

    ok(true, "Report breakage view was shown");

    viewShown = BrowserTestUtils.waitForEvent(siteNotWorkingView, "ViewShown");
    let cancelButton = document.getElementById(
      "protections-popup-sendReportView-cancel"
    );
    cancelButton.click();
    await viewShown;

    ok(true, "Main view was shown");
  });

  Services.prefs.clearUserPref(TP_PREF);
});

add_task(async function testReportBreakageSiteException() {
  Services.prefs.setBoolPref(TP_PREF, true);

  let url = TRACKING_PAGE + "?a=b&1=abc&unicode=🦊";

  await BrowserTestUtils.withNewTab(url, async browser => {
    let loaded = BrowserTestUtils.browserLoaded(browser, false);
    gProtectionsHandler.disableForCurrentPage();
    await loaded;

    await openProtectionsPanel();

    let siteFixedButton = document.getElementById(
      "protections-popup-tp-switch-breakage-fixed-link"
    );
    ok(
      BrowserTestUtils.is_visible(siteFixedButton),
      "site fixed button is visible"
    );
    let sendReportView = document.getElementById(
      "protections-popup-sendReportView"
    );
    let viewShown = BrowserTestUtils.waitForEvent(sendReportView, "ViewShown");
    siteFixedButton.click();
    await viewShown;

    ok(true, "Report breakage view was shown");

    await testReportBreakageSubmit(
      TRACKING_PAGE,
      "trackingprotection",
      false,
      true
    );

    // Pass false for shouldReload - there's no need since the tab is going away.
    gProtectionsHandler.enableForCurrentPage(false);
  });

  Services.prefs.clearUserPref(TP_PREF);
});

add_task(async function testNoTracking() {
  await BrowserTestUtils.withNewTab(BENIGN_PAGE, async function() {
    await openProtectionsPanel();

    let siteNotWorkingButton = document.getElementById(
      "protections-popup-tp-switch-breakage-link"
    );
    ok(
      BrowserTestUtils.is_hidden(siteNotWorkingButton),
      "site not working button is not visible"
    );
  });
});

add_task(async function testReportBreakageError() {
  Services.prefs.setBoolPref(TP_PREF, true);
  // Make sure that we correctly strip the query.
  let url = TRACKING_PAGE + "?a=b&1=abc&unicode=🦊";
  await BrowserTestUtils.withNewTab(url, async function() {
    await openAndTestReportBreakage(TRACKING_PAGE, "trackingprotection", true);
  });

  Services.prefs.clearUserPref(TP_PREF);
});

add_task(async function testTP() {
  Services.prefs.setBoolPref(TP_PREF, true);
  // Make sure that we correctly strip the query.
  let url = TRACKING_PAGE + "?a=b&1=abc&unicode=🦊";
  await BrowserTestUtils.withNewTab(url, async function() {
    await openAndTestReportBreakage(TRACKING_PAGE, "trackingprotection");
  });

  Services.prefs.clearUserPref(TP_PREF);
});

add_task(async function testCR() {
  Services.prefs.setIntPref(
    CB_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  // Make sure that we correctly strip the query.
  let url = COOKIE_PAGE + "?a=b&1=abc&unicode=🦊";
  await BrowserTestUtils.withNewTab(url, async function() {
    await openAndTestReportBreakage(COOKIE_PAGE, "cookierestrictions");
  });

  Services.prefs.clearUserPref(CB_PREF);
});

add_task(async function testFP() {
  Services.prefs.setIntPref(CB_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
  Services.prefs.setBoolPref(FP_PREF, true);
  // Make sure that we correctly strip the query.
  let url = TRACKING_PAGE + "?a=b&1=abc&unicode=🦊";
  await BrowserTestUtils.withNewTab(url, async function(browser) {
    let promise = waitForContentBlockingEvent();
    await SpecialPowers.spawn(browser, [], function() {
      content.postMessage("fingerprinting", "*");
    });
    await promise;

    await openAndTestReportBreakage(TRACKING_PAGE, "fingerprinting", true);
  });

  Services.prefs.clearUserPref(FP_PREF);
  Services.prefs.clearUserPref(CB_PREF);
});

add_task(async function testCM() {
  Services.prefs.setIntPref(CB_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
  Services.prefs.setBoolPref(CM_PREF, true);
  // Make sure that we correctly strip the query.
  let url = TRACKING_PAGE + "?a=b&1=abc&unicode=🦊";
  await BrowserTestUtils.withNewTab(url, async function(browser) {
    let promise = waitForContentBlockingEvent();
    await SpecialPowers.spawn(browser, [], function() {
      content.postMessage("cryptomining", "*");
    });
    await promise;

    await openAndTestReportBreakage(TRACKING_PAGE, "cryptomining", true);
  });

  Services.prefs.clearUserPref(CM_PREF);
  Services.prefs.clearUserPref(CB_PREF);
});

async function openAndTestReportBreakage(url, tags, error = false) {
  await openProtectionsPanel();

  let siteNotWorkingButton = document.getElementById(
    "protections-popup-tp-switch-breakage-link"
  );
  ok(
    BrowserTestUtils.is_visible(siteNotWorkingButton),
    "site not working button is visible"
  );
  let siteNotWorkingView = document.getElementById(
    "protections-popup-siteNotWorkingView"
  );
  let viewShown = BrowserTestUtils.waitForEvent(
    siteNotWorkingView,
    "ViewShown"
  );
  siteNotWorkingButton.click();
  await viewShown;

  let sendReportButton = document.getElementById(
    "protections-popup-siteNotWorkingView-sendReport"
  );
  let sendReportView = document.getElementById(
    "protections-popup-sendReportView"
  );
  viewShown = BrowserTestUtils.waitForEvent(sendReportView, "ViewShown");
  sendReportButton.click();
  await viewShown;

  ok(true, "Report breakage view was shown");

  await testReportBreakageSubmit(url, tags, error, false);
}

// This function assumes that the breakage report view is ready.
async function testReportBreakageSubmit(url, tags, error, hasException) {
  // Setup a mock server for receiving breakage reports.
  let server = new HttpServer();
  server.start(-1);
  let i = server.identity;
  let path =
    i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort + "/";

  Services.prefs.setStringPref(PREF_REPORT_BREAKAGE_URL, path);

  let comments = document.getElementById(
    "protections-popup-sendReportView-collection-comments"
  );
  is(comments.value, "", "Comments textarea should initially be empty");

  let submitButton = document.getElementById(
    "protections-popup-sendReportView-submit"
  );
  let reportURL = document.getElementById(
    "protections-popup-sendReportView-collection-url"
  ).value;

  is(reportURL, url, "Shows the correct URL in the report UI.");

  // Make sure that sending the report closes the identity popup.
  let popuphidden = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );

  // Check that we're receiving a good report.
  await new Promise(resolve => {
    server.registerPathHandler("/", async (request, response) => {
      is(request.method, "POST", "request was a post");

      // Extract and "parse" the form data in the request body.
      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let boundary = request
        .getHeader("Content-Type")
        .match(/boundary=-+([^-]*)/i)[1];
      let regex = new RegExp("-+" + boundary + "-*\\s+");
      let sections = body.split(regex);

      let prefs = [
        "privacy.trackingprotection.enabled",
        "privacy.trackingprotection.pbmode.enabled",
        "urlclassifier.trackingTable",
        "network.http.referer.defaultPolicy",
        "network.http.referer.defaultPolicy.pbmode",
        "network.cookie.cookieBehavior",
        "network.cookie.lifetimePolicy",
        "privacy.annotate_channels.strict_list.enabled",
        "privacy.restrict3rdpartystorage.expiration",
        "privacy.trackingprotection.fingerprinting.enabled",
        "privacy.trackingprotection.cryptomining.enabled",
      ];
      let prefsBody = "";

      for (let pref of prefs) {
        prefsBody += `${pref}: ${Preferences.get(pref)}\r\n`;
      }

      Assert.deepEqual(
        sections,
        [
          "",
          `Content-Disposition: form-data; name=\"title\"\r\n\r\n${
            Services.io.newURI(reportURL).host
          }\r\n`,
          'Content-Disposition: form-data; name="body"\r\n\r\n' +
            `Full URL: ${reportURL + "?"}\r\n` +
            `userAgent: ${navigator.userAgent}\r\n\r\n` +
            "**Preferences**\r\n" +
            `${prefsBody}\r\n` +
            `hasException: ${hasException}\r\n\r\n` +
            "**Comments**\r\n" +
            "This is a comment\r\n",
          'Content-Disposition: form-data; name="labels"\r\n\r\n' +
            `${hasException ? "" : tags}\r\n`,
          "",
        ],
        "Should send the correct form data"
      );

      if (error) {
        response.setStatusLine(request.httpVersion, 500, "Request failed");
      } else {
        response.setStatusLine(request.httpVersion, 201, "Entry created");
      }

      resolve();
    });

    comments.value = "This is a comment";
    submitButton.click();
  });

  let errorMessage = document.getElementById(
    "protections-popup-sendReportView-report-error"
  );
  if (error) {
    await TestUtils.waitForCondition(() =>
      BrowserTestUtils.is_visible(errorMessage)
    );
    is(
      comments.value,
      "This is a comment",
      "Comment not cleared in case of an error"
    );
    gProtectionsHandler._protectionsPopup.hidePopup();
  } else {
    ok(BrowserTestUtils.is_hidden(errorMessage), "Error message not shown");
  }

  await popuphidden;

  // Stop the server.
  await new Promise(r => server.stop(r));

  Services.prefs.clearUserPref(PREF_REPORT_BREAKAGE_URL);
}
