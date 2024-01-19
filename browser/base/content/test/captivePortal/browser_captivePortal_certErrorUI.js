/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["captivedetect.canonicalURL", CANONICAL_URL],
      ["captivedetect.canonicalContent", CANONICAL_CONTENT],
    ],
  });
});

// This tests the alternate cert error UI when we are behind a captive portal.
add_task(async function checkCaptivePortalCertErrorUI() {
  info(
    "Checking that the alternate cert error UI is shown when we are behind a captive portal"
  );

  // Open a second window in the background. Later, we'll check that
  // when we click the button to open the captive portal tab, the tab
  // only opens in the active window and not in the background one.
  let secondWindow = await openWindowAndWaitForFocus();
  await SimpleTest.promiseFocus(window);

  await portalDetected();

  // Check that we didn't open anything in the background window.
  ensureNoPortalTab(secondWindow);

  let tab = await openCaptivePortalErrorTab();
  let browser = tab.linkedBrowser;
  let portalTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    CANONICAL_URL
  );

  await SpecialPowers.spawn(browser, [], async () => {
    let doc = content.document;
    let loginButton = doc.getElementById("openPortalLoginPageButton");
    await ContentTaskUtils.waitForCondition(
      () => ContentTaskUtils.isVisible(loginButton),
      "Captive portal error page UI is visible"
    );

    if (!Services.focus.focusedElement == loginButton) {
      await ContentTaskUtils.waitForEvent(loginButton, "focus");
    }

    Assert.ok(true, "openPortalLoginPageButton has focus");
    info("Clicking the Open Login Page button");
    await EventUtils.synthesizeMouseAtCenter(loginButton, {}, content);
  });

  let portalTab = await portalTabPromise;
  is(
    gBrowser.selectedTab,
    portalTab,
    "Login page should be open in a new foreground tab."
  );

  // Check that we didn't open anything in the background window.
  ensureNoPortalTab(secondWindow);

  // Make sure clicking the "Open Login Page" button again focuses the existing portal tab.
  await BrowserTestUtils.switchTab(gBrowser, tab);
  // Passing an empty function to BrowserTestUtils.switchTab lets us wait for an arbitrary
  // tab switch.
  portalTabPromise = BrowserTestUtils.switchTab(gBrowser, () => {});
  await SpecialPowers.spawn(browser, [], async () => {
    info("Clicking the Open Login Page button.");
    let loginButton = content.document.getElementById(
      "openPortalLoginPageButton"
    );
    await EventUtils.synthesizeMouseAtCenter(loginButton, {}, content);
  });

  info("Opening captive portal login page");
  let portalTab2 = await portalTabPromise;
  is(portalTab2, portalTab, "The existing portal tab should be focused.");

  // Check that we didn't open anything in the background window.
  ensureNoPortalTab(secondWindow);

  let portalTabClosing = BrowserTestUtils.waitForTabClosing(portalTab);
  let errorTabReloaded = BrowserTestUtils.waitForErrorPage(browser);

  Services.obs.notifyObservers(null, "captive-portal-login-success");
  await portalTabClosing;

  info(
    "Waiting for error tab to be reloaded after the captive portal was freed."
  );
  await errorTabReloaded;
  await SpecialPowers.spawn(browser, [], () => {
    let doc = content.document;
    ok(
      !doc.body.classList.contains("captiveportal"),
      "Captive portal error page UI is not visible."
    );
  });

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(secondWindow);
});

add_task(async function testCaptivePortalAdvancedPanel() {
  info(
    "Checking that the advanced section of the about:certerror UI is shown when we are behind a captive portal."
  );
  await portalDetected();
  let tab = await openCaptivePortalErrorTab();
  let browser = tab.linkedBrowser;

  const waitForLocationChange = (async () => {
    await BrowserTestUtils.waitForLocationChange(gBrowser, BAD_CERT_PAGE);
    info("(waitForLocationChange resolved)");
  })();
  await SpecialPowers.spawn(browser, [BAD_CERT_PAGE], async expectedURL => {
    const doc = content.document;
    let advancedButton = doc.getElementById("advancedButton");
    await ContentTaskUtils.waitForCondition(
      () => ContentTaskUtils.isVisible(advancedButton),
      "Captive portal UI is visible"
    );

    info("Clicking on the advanced button");
    const advPanel = doc.getElementById("badCertAdvancedPanel");
    ok(
      !ContentTaskUtils.isVisible(advPanel),
      "Advanced panel is not yet visible"
    );
    await EventUtils.synthesizeMouseAtCenter(advancedButton, {}, content);
    ok(ContentTaskUtils.isVisible(advPanel), "Advanced panel is now visible");

    let advPanelContent = doc.getElementById("badCertTechnicalInfo");
    ok(
      ContentTaskUtils.isVisible(advPanelContent) &&
        advPanelContent.textContent.includes("expired.example.com"),
      "Advanced panel text content is visible"
    );

    let advPanelErrorCode = doc.getElementById("errorCode");
    ok(
      advPanelErrorCode.textContent,
      "Cert error code is visible in the advanced panel"
    );

    // -

    const advPanelExceptionButton = doc.getElementById("exceptionDialogButton");

    function isOnCertErrorPage() {
      return ContentTaskUtils.isVisible(advPanel);
    }

    ok(isOnCertErrorPage(), "On cert error page before adding exception");
    ok(
      advPanelExceptionButton.disabled,
      "Exception button should start disabled"
    );
    await EventUtils.synthesizeMouseAtCenter(
      advPanelExceptionButton,
      {},
      content
    ); // Click
    const clickTime = content.performance.now();
    ok(
      isOnCertErrorPage(),
      "Still on cert error page because clicked too early"
    );

    // Now waitForCondition now that it's possible.
    try {
      await ContentTaskUtils.waitForCondition(
        () => !advPanelExceptionButton.disabled,
        "Wait for exception button enabled"
      );
    } catch (rejected) {
      ok(false, rejected);
      return;
    }
    ok(
      !advPanelExceptionButton.disabled,
      "Exception button should be enabled after waiting"
    );
    const msSinceClick = content.performance.now() - clickTime;
    const expr = `${msSinceClick} > 1000`;
    /* eslint-disable no-eval */
    ok(eval(expr), `Exception button should stay disabled for ${expr} ms`);

    await EventUtils.synthesizeMouseAtCenter(
      advPanelExceptionButton,
      {},
      content
    ); // Click
    info("Clicked");
  });
  await waitForLocationChange;
  info("Page reloaded after adding cert exception");

  // Clear the certificate exception.
  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  certOverrideService.clearValidityOverride("expired.example.com", -1, {});

  info("After clearing cert override, asking for reload...");
  const waitForErrorPage = BrowserTestUtils.waitForErrorPage(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    info("reload...");
    content.location.reload();
  });
  info("waitForErrorPage...");
  await waitForErrorPage;

  info("removeTab...");
  await BrowserTestUtils.removeTab(tab);
  info("Done!");
});
