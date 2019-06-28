/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_MITM_PRIMING = "security.certerrors.mitm.priming.enabled";
const PREF_MITM_PRIMING_ENDPOINT = "security.certerrors.mitm.priming.endpoint";
const PREF_MITM_CANARY_ISSUER = "security.pki.mitm_canary_issuer";
const PREF_MITM_AUTO_ENABLE_ENTERPRISE_ROOTS = "security.certerrors.mitm.auto_enable_enterprise_roots";
const PREF_ENTERPRISE_ROOTS = "security.enterprise_roots.enabled";

const UNKNOWN_ISSUER = "https://untrusted.example.com";

// Check that basic MitM priming works and the MitM error page is displayed successfully.
add_task(async function checkMitmPriming() {
  await SpecialPowers.pushPrefEnv({"set": [
    [PREF_MITM_PRIMING, true],
    [PREF_MITM_PRIMING_ENDPOINT, UNKNOWN_ISSUER],
  ]});

  let browser;
  let certErrorLoaded;
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, UNKNOWN_ISSUER);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
  }, false);

  await certErrorLoaded;

  // The page will reload after the initial canary request, so we'll just
  // wait until we're seeing the dedicated MitM page.
  await TestUtils.waitForCondition(function() {
    return ContentTask.spawn(browser, {}, () => {
      return content.document.body.getAttribute("code") == "MOZILLA_PKIX_ERROR_MITM_DETECTED";
    });
  }, "Loads the MitM error page.");

  ok(true, "Successfully loaded the MitM error page.");

  is(Services.prefs.getStringPref(PREF_MITM_CANARY_ISSUER), "CN=Unknown CA", "Stored the correct issuer");

  await ContentTask.spawn(browser, {}, () => {
    let mitmName1 = content.document.querySelector("#errorShortDescText .mitm-name");
    ok(ContentTaskUtils.is_visible(mitmName1), "Potential man in the middle is displayed");
    is(mitmName1.textContent, "Unknown CA", "Shows the name of the issuer.");

    let mitmName2 = content.document.querySelector("#errorWhatToDoText .mitm-name");
    ok(ContentTaskUtils.is_visible(mitmName2), "Potential man in the middle is displayed");
    is(mitmName2.textContent, "Unknown CA", "Shows the name of the issuer.");
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref(PREF_MITM_CANARY_ISSUER);
});

// Check that we set the enterprise roots pref correctly on MitM
add_task(async function checkMitmAutoEnableEnterpriseRoots() {
  await SpecialPowers.pushPrefEnv({"set": [
    [PREF_MITM_PRIMING, true],
    [PREF_MITM_PRIMING_ENDPOINT, UNKNOWN_ISSUER],
    [PREF_MITM_AUTO_ENABLE_ENTERPRISE_ROOTS, true],
    [PREF_ENTERPRISE_ROOTS, false],
  ]});

  let browser;
  let certErrorLoaded;

  let prefChanged = TestUtils.waitForPrefChange(PREF_ENTERPRISE_ROOTS, value => value === true);
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, UNKNOWN_ISSUER);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
  }, false);

  await certErrorLoaded;
  await prefChanged;

  // The page will reload after the initial canary request, so we'll just
  // wait until we're seeing the dedicated MitM page.
  await TestUtils.waitForCondition(function() {
    return ContentTask.spawn(browser, {}, () => {
      return content.document.body.getAttribute("code") == "MOZILLA_PKIX_ERROR_MITM_DETECTED";
    });
  }, "Loads the MitM error page.");

  ok(true, "Successfully loaded the MitM error page.");

  ok(!Services.prefs.prefHasUserValue(PREF_ENTERPRISE_ROOTS), "Flipped the enterprise roots pref back");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref(PREF_MITM_CANARY_ISSUER);
});
