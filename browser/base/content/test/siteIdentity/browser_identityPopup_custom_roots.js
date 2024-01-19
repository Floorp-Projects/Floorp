/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that the UI for imported root certificates shows up correctly in the identity popup.
 */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

// This test is incredibly simple, because our test framework already
// imports root certificates by default, so we just visit example.com
// and verify that the custom root certificates UI is visible.
add_task(async function test_https() {
  await BrowserTestUtils.withNewTab("https://example.com", async function () {
    let promisePanelOpen = BrowserTestUtils.waitForEvent(
      window,
      "popupshown",
      true,
      event => event.target == gIdentityHandler._identityPopup
    );

    gIdentityHandler._identityIconBox.click();
    await promisePanelOpen;
    let customRootWarning = document.getElementById(
      "identity-popup-security-decription-custom-root"
    );
    ok(
      BrowserTestUtils.isVisible(customRootWarning),
      "custom root warning is visible"
    );

    let securityView = document.getElementById("identity-popup-securityView");
    let shown = BrowserTestUtils.waitForEvent(securityView, "ViewShown");
    document.getElementById("identity-popup-security-button").click();
    await shown;

    let subPanelInfo = document.getElementById(
      "identity-popup-content-verifier-unknown"
    );
    ok(
      BrowserTestUtils.isVisible(subPanelInfo),
      "custom root warning in sub panel is visible"
    );
  });
});

// Also check that there are conditions where this isn't shown.
add_task(async function test_http() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.withNewTab("http://example.com", async function () {
    let promisePanelOpen = BrowserTestUtils.waitForEvent(
      window,
      "popupshown",
      true,
      event => event.target == gIdentityHandler._identityPopup
    );
    gIdentityHandler._identityIconBox.click();
    await promisePanelOpen;
    let customRootWarning = document.getElementById(
      "identity-popup-security-decription-custom-root"
    );
    ok(
      BrowserTestUtils.isHidden(customRootWarning),
      "custom root warning is hidden"
    );

    let securityView = document.getElementById("identity-popup-securityView");
    let shown = BrowserTestUtils.waitForEvent(securityView, "ViewShown");
    document.getElementById("identity-popup-security-button").click();
    await shown;

    let subPanelInfo = document.getElementById(
      "identity-popup-content-verifier-unknown"
    );
    ok(
      BrowserTestUtils.isHidden(subPanelInfo),
      "custom root warning in sub panel is hidden"
    );
  });
});
