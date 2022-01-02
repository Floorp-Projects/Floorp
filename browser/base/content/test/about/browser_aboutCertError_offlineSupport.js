/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BAD_CERT_PAGE = "https://expired.example.com";
const DUMMY_SUPPORT_BASE_PATH = "/1/firefox/fxVersion/OSVersion/language/";
const DUMMY_SUPPORT_URL = BAD_CERT_PAGE + DUMMY_SUPPORT_BASE_PATH;
const OFFLINE_SUPPORT_PAGE =
  "chrome://browser/content/certerror/supportpages/time-errors.html";

add_task(async function testOfflineSupportPage() {
  // Cache the original value of app.support.baseURL pref to reset later
  let originalBaseURL = Services.prefs.getCharPref("app.support.baseURL");

  Services.prefs.setCharPref("app.support.baseURL", DUMMY_SUPPORT_URL);
  let errorTab = await openErrorPage(BAD_CERT_PAGE);

  let offlineSupportPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    DUMMY_SUPPORT_URL + "time-errors"
  );
  await SpecialPowers.spawn(
    errorTab.linkedBrowser,
    [DUMMY_SUPPORT_URL],
    async expectedURL => {
      let doc = content.document;

      let learnMoreLink = doc.getElementById("learnMoreLink");
      let supportPageURL = learnMoreLink.getAttribute("href");
      ok(
        supportPageURL == expectedURL + "time-errors",
        "Correct support page URL has been set"
      );
      await EventUtils.synthesizeMouseAtCenter(learnMoreLink, {}, content);
    }
  );
  let offlineSupportTab = await offlineSupportPromise;
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    OFFLINE_SUPPORT_PAGE
  );

  // Reset this pref instead of clearing it to maintain globally set
  // custom value for testing purposes.
  Services.prefs.setCharPref("app.support.baseURL", originalBaseURL);

  await BrowserTestUtils.removeTab(offlineSupportTab);
  await BrowserTestUtils.removeTab(errorTab);
});
