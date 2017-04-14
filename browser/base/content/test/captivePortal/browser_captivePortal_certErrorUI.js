/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BAD_CERT_PAGE = "https://expired.example.com/";

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
    certErrorLoaded = BrowserTestUtils.waitForContentEvent(browser, "DOMContentLoaded");
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
    let loginButton = doc.getElementById("openPortalLoginPageButton");
    is(loginButton.getAttribute("autofocus"), "true", "openPortalLoginPageButton has autofocus");
    loginButton.click();
  });

  let portalTab = yield portalTabPromise;
  is(gBrowser.selectedTab, portalTab, "Login page should be open in a new foreground tab.");

  // Make sure clicking the "Open Login Page" button again focuses the existing portal tab.
  yield BrowserTestUtils.switchTab(gBrowser, errorTab);
  // Passing an empty function to BrowserTestUtils.switchTab lets us wait for an arbitrary
  // tab switch.
  portalTabPromise = BrowserTestUtils.switchTab(gBrowser, () => {});
  yield ContentTask.spawn(browser, null, () => {
    info("Clicking the Open Login Page button.");
    content.document.getElementById("openPortalLoginPageButton").click();
  });

  let portalTab2 = yield portalTabPromise;
  is(portalTab2, portalTab, "The existing portal tab should be focused.");

  let portalTabRemoved = BrowserTestUtils.tabRemoved(portalTab);
  let errorTabReloaded = BrowserTestUtils.waitForErrorPage(browser);

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
