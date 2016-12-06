/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BAD_CERT_PAGE = "https://expired.example.com/";

const CANONICAL_CONTENT = "success";
const CANONICAL_URL = "data:text/plain;charset=utf-8," + CANONICAL_CONTENT;

// This tests the alternate cert error UI when we are behind a captive portal.

add_task(function* checkCaptivePortalCertErrorUI() {
  yield SpecialPowers.pushPrefEnv({
    set: [["captivedetect.canonicalURL", CANONICAL_URL],
          ["captivedetect.canonicalContent", CANONICAL_CONTENT]],
  });

  let captivePortalStatePropagated = TestUtils.topicObserved("ipc:network:captive-portal-set-state");

  info("Checking that the alternate about:certerror UI is shown when we are behind a captive portal.");
  Services.obs.notifyObservers(null, "captive-portal-login", null);

  info("Waiting for captive portal state to be propagated to the content process.");
  yield captivePortalStatePropagated;

  // Open a page with a cert error.
  let browser;
  let certErrorLoaded;
  let errorTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    let tab = gBrowser.addTab(BAD_CERT_PAGE);
    gBrowser.selectedTab = tab;
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = waitForCertErrorLoad(browser);
    return tab;
  }, false);

  info("Waiting for cert error page to load.")
  yield certErrorLoaded;

  let portalTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, CANONICAL_URL);

  yield ContentTask.spawn(browser, null, () => {
    let doc = content.document;
    ok(doc.body.classList.contains("captiveportal"),
       "Captive portal error page UI is visible.");

    info("Clicking the Open Login Page button.");
    doc.getElementById("openPortalLoginPageButton").click();
  });

  let portalTab = yield portalTabPromise;
  is(gBrowser.selectedTab, portalTab, "Login page should be open in a new foreground tab.");

  let portalTabRemoved = BrowserTestUtils.removeTab(portalTab, {dontRemove: true});
  let errorTabReloaded = waitForCertErrorLoad(browser);

  Services.obs.notifyObservers(null, "captive-portal-login-success", null);
  yield portalTabRemoved;

  info("Waiting for error tab to be reloaded after the captive portal was freed.");
  yield errorTabReloaded;
  yield ContentTask.spawn(browser, null, () => {
    let doc = content.document;
    ok(!doc.body.classList.contains("captiveportal"),
       "Captive portal error page UI is not visible.");
  });

  yield BrowserTestUtils.removeTab(errorTab);
});
