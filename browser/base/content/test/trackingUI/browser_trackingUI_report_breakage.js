/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const COOKIE_PAGE = "http://not-tracking.example.com/browser/browser/base/content/test/trackingUI/cookiePage.html";

const CB_PREF = "browser.contentblocking.enabled";
const CB_UI_PREF = "browser.contentblocking.ui.enabled";
const TP_PREF = "privacy.trackingprotection.enabled";
const PREF_REPORT_BREAKAGE_ENABLED = "browser.contentblocking.reportBreakage.enabled";
const PREF_REPORT_BREAKAGE_URL = "browser.contentblocking.reportBreakage.url";

let {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js", {});
let {CommonUtils} = ChromeUtils.import("resource://services-common/utils.js", {});
let {Preferences} = ChromeUtils.import("resource://gre/modules/Preferences.jsm", {});

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
  ]});
  await UrlClassifierTestUtils.addTestTrackers();
});

function openIdentityPopup() {
  let mainView = document.getElementById("identity-popup-mainView");
  let viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  gIdentityHandler._identityBox.click();
  return viewShown;
}

add_task(async function testReportBreakageVisibility() {
  let scenarios = [
    {
      url: TRACKING_PAGE,
      prefs: {
        "browser.contentblocking.enabled": true,
        "privacy.trackingprotection.enabled": true,
        "browser.contentblocking.reportBreakage.enabled": true,
      },
      buttonVisible: true,
    },
    {
      url: TRACKING_PAGE,
      hasException: true,
      prefs: {
        "browser.contentblocking.enabled": true,
        "privacy.trackingprotection.enabled": true,
        "browser.contentblocking.reportBreakage.enabled": true,
      },
      buttonVisible: true,
    },
    {
      url: TRACKING_PAGE,
      prefs: {
        "browser.contentblocking.enabled": false,
        "privacy.trackingprotection.enabled": true,
        "browser.contentblocking.reportBreakage.enabled": true,
      },
      buttonVisible: false,
    },
    {
      url: TRACKING_PAGE,
      prefs: {
        "browser.contentblocking.enabled": true,
        "privacy.trackingprotection.enabled": true,
        "browser.contentblocking.reportBreakage.enabled": false,
      },
      buttonVisible: false,
    },
    {
      url: BENIGN_PAGE,
      prefs: {
        "browser.contentblocking.enabled": true,
        "privacy.trackingprotection.enabled": true,
        "browser.contentblocking.reportBreakage.enabled": true,
      },
      buttonVisible: false,
    },
    {
      url: COOKIE_PAGE,
      prefs: {
        "browser.contentblocking.enabled": true,
        "browser.fastblock.enabled": false,
        "privacy.trackingprotection.enabled": false,
        "network.cookie.cookieBehavior": Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
        "browser.contentblocking.reportBreakage.enabled": false,
        "browser.contentblocking.rejecttrackers.reportBreakage.enabled": true,
      },
      buttonVisible: true,
    },
  ];

  for (let scenario of scenarios) {
    for (let pref in scenario.prefs) {
      Preferences.set(pref, scenario.prefs[pref]);
    }

    let uri = Services.io.newURI(scenario.url);
    if (scenario.hasException) {
      Services.perms.add(uri, "trackingprotection", Services.perms.ALLOW_ACTION);
    }

    await BrowserTestUtils.withNewTab(scenario.url, async function() {
      await openIdentityPopup();

      let reportBreakageButton = document.getElementById("identity-popup-content-blocking-report-breakage");
      is(BrowserTestUtils.is_visible(reportBreakageButton), scenario.buttonVisible,
        "report breakage button has the correct visibility");
    });

    Services.perms.remove(uri, "trackingprotection");
    for (let pref in scenario.prefs) {
      Services.prefs.clearUserPref(pref);
    }
  }
});

add_task(async function testReportBreakageCancel() {
  Services.prefs.setBoolPref(TP_PREF, true);
  Services.prefs.setBoolPref(CB_PREF, true);
  Services.prefs.setBoolPref(PREF_REPORT_BREAKAGE_ENABLED, true);

  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function() {
    await openIdentityPopup();

    let reportBreakageButton = document.getElementById("identity-popup-content-blocking-report-breakage");
    ok(BrowserTestUtils.is_visible(reportBreakageButton), "report breakage button is visible");
    let reportBreakageView = document.getElementById("identity-popup-breakageReportView");
    let viewShown = BrowserTestUtils.waitForEvent(reportBreakageView, "ViewShown");
    reportBreakageButton.click();
    await viewShown;

    ok(true, "Report breakage view was shown");

    let mainView = document.getElementById("identity-popup-mainView");
    viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
    let cancelButton = document.getElementById("identity-popup-breakageReportView-cancel");
    cancelButton.click();
    await viewShown;

    ok(true, "Main view was shown");
  });

  Services.prefs.clearUserPref(CB_PREF);
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(PREF_REPORT_BREAKAGE_ENABLED);
});

add_task(async function testReportBreakage() {
  // Setup a mock server for receiving breakage reports.
  let server = new HttpServer();
  server.start(-1);
  let i = server.identity;
  let path = i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort + "/";

  Services.prefs.setBoolPref(TP_PREF, true);
  Services.prefs.setBoolPref(CB_PREF, true);
  Services.prefs.setBoolPref(PREF_REPORT_BREAKAGE_ENABLED, true);
  Services.prefs.setStringPref(PREF_REPORT_BREAKAGE_URL, path);

  // Make sure that we correctly strip the query.
  let url = TRACKING_PAGE + "?a=b&1=abc&unicode=🦊";
  await BrowserTestUtils.withNewTab(url, async function() {
    await openIdentityPopup();

    let reportBreakageButton = document.getElementById("identity-popup-content-blocking-report-breakage");
    ok(BrowserTestUtils.is_visible(reportBreakageButton), "report breakage button is visible");
    let reportBreakageView = document.getElementById("identity-popup-breakageReportView");
    let viewShown = BrowserTestUtils.waitForEvent(reportBreakageView, "ViewShown");
    reportBreakageButton.click();
    await viewShown;

    let submitButton = document.getElementById("identity-popup-breakageReportView-submit");
    let reportURL = document.getElementById("identity-popup-breakageReportView-collection-url").textContent;

    is(reportURL, TRACKING_PAGE, "Shows the correct URL in the report UI.");

    // Make sure that sending the report closes the identity popup.
    let popuphidden = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");

    // Check that we're receiving a good report.
    await new Promise(resolve => {
      server.registerPathHandler("/", async (request, response) => {
        is(request.method, "POST", "request was a post");

        // Extract and "parse" the form data in the request body.
        let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
        let boundary = request.getHeader("Content-Type").match(/boundary=-+([^-]*)/i)[1];
        let regex = new RegExp("-+" + boundary + "-*\\s+");
        let sections = body.split(regex);

        let prefs = [
          "privacy.trackingprotection.enabled",
          "privacy.trackingprotection.pbmode.enabled",
          "browser.contentblocking.trackingprotection.control-center.ui.enabled",
          "urlclassifier.trackingTable",
          "network.http.referer.defaultPolicy",
          "network.http.referer.defaultPolicy.pbmode",
          "browser.contentblocking.rejecttrackers.control-center.ui.enabled",
          "network.cookie.cookieBehavior",
          "network.cookie.lifetimePolicy",
          "privacy.restrict3rdpartystorage.expiration",
          "browser.fastblock.enabled",
          "browser.contentblocking.fastblock.control-center.ui.enabled",
          "browser.fastblock.timeout",
        ];
        let prefsBody = "";

        for (let pref of prefs) {
          prefsBody += `${pref}: ${Preferences.get(pref)}\r\n`;
        }

        Assert.deepEqual(sections, [
          "",
          "Content-Disposition: form-data; name=\"title\"\r\n\r\ntracking.example.org\r\n",
          "Content-Disposition: form-data; name=\"body\"\r\n\r\n" +
          `Full URL: ${reportURL + "?"}\r\n` +
          `userAgent: ${navigator.userAgent}\r\n\r\n` +
          "**Preferences**\r\n" +
          `${prefsBody}\r\n` +
          "**Comments**\r\n" +
          "This is a comment\r\n",
          "",
        ], "Should send the correct form data");

        resolve();
      });

      let comments = document.getElementById("identity-popup-breakageReportView-collection-comments");
      comments.value = "This is a comment";
      submitButton.click();
    });

    await popuphidden;
  });

  // Stop the server.
  await new Promise(r => server.stop(r));

  Services.prefs.clearUserPref(CB_PREF);
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(PREF_REPORT_BREAKAGE_ENABLED);
  Services.prefs.clearUserPref(PREF_REPORT_BREAKAGE_URL);
});

add_task(async function cleanup() {
  // Clear prefs that are touched in this test again for sanity.
  Services.prefs.clearUserPref(CB_PREF);
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(PREF_REPORT_BREAKAGE_ENABLED);
  Services.prefs.clearUserPref(PREF_REPORT_BREAKAGE_URL);

  UrlClassifierTestUtils.cleanupTestTrackers();
});
