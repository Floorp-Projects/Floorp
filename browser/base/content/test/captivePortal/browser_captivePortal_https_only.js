/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);
const testPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);
const CANONICAL_URI = Services.io.newURI(testPath);
const PERMISSION_NAME = "https-only-load-insecure";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    // That changes the canoncicalURL from "http://{server}/captive-detect/success.txt"
    // to http://example.com
    set: [
      ["captivedetect.canonicalURL", testPath],
      ["dom.security.https_only_mode", true],
    ],
  });
});

// This test checks if https-only exempts the canoncial uri.
add_task(async function checkCaptivePortalExempt() {
  await portalDetected();
  info("Checking that the canonical uri is exempt by https-only mode");
  let tab = await openCaptivePortalErrorTab();
  let browser = tab.linkedBrowser;
  let portalTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, testPath);

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
  is(
    PermissionTestUtils.testPermission(CANONICAL_URI, PERMISSION_NAME),
    Services.perms.ALLOW_ACTION,
    "Check permission in perm. manager if canoncial uri is set as exempt."
  );
  let portalTab = await portalTabPromise;
  is(
    gBrowser.selectedTab,
    portalTab,
    "Login page should be open in a new foreground tab."
  );
  is(
    gBrowser.currentURI.spec,
    testPath,
    "Opened the right URL without upgrading it."
  );
  // Close all tabs
  await BrowserTestUtils.removeTab(portalTab);
  let tabReloaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  Services.obs.notifyObservers(null, "captive-portal-login-success");
  await tabReloaded;
  await BrowserTestUtils.removeTab(tab);
});
