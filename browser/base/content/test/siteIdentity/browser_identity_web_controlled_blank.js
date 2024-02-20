/*
 * This work is marked with CC0 1.0. To view a copy of this license,
 * visit http://creativecommons.org/publicdomain/zero/1.0
 *
 *
 * Tests for correct behaviour of web-controlled about:blank pages in identity panel.
 * Getting into a testable state is different enough that we test all the behaviors here
 * separately, rather than with the usual (secure, insecure, etc.) cases
 */

const TEST_HOST = "example.com";
const TEST_ORIGIN = "https://" + TEST_HOST;
const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  TEST_ORIGIN
);
const LOCALHOST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://127.0.0.1:8888"
);
const TEST_URI = TEST_PATH + "test_web_controlled_blank.html";
const DUMMY_URI = LOCALHOST_PATH + "dummy_page.html";

// Open a new tab of `test_web_controlled_blank.html` and click
// an element with a particular ID to open an about:blank popup
// that is controlled by that first tab.
//
// Then test that the UI elements are all correct. And make sure
// we don't have anything odd going on after navigating away in the
// popup.
async function web_controlled_about_blank_helper(id_to_click) {
  // Open a new tab that will control about:blank pages
  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Open a new popup by clicking the provided id_to_click from the content
    let popupWindowPromise = BrowserTestUtils.waitForNewWindow();
    await SpecialPowers.spawn(browser, [id_to_click], async function (id) {
      content.document.getElementById(id).click();
    });
    let popupWindow = await popupWindowPromise;

    // Validate the icon in the urlbar
    let identityIcon = popupWindow.document.querySelector("#identity-icon");
    let identityIconImageURL = popupWindow
      .getComputedStyle(identityIcon)
      .getPropertyValue("list-style-image");
    is(
      identityIconImageURL,
      `url("chrome://global/skin/icons/info.svg")`,
      "The identity icon has a correct image url."
    );

    // Open the identity panel
    let popupShown = BrowserTestUtils.waitForEvent(
      popupWindow,
      "popupshown",
      true,
      event => event.target == popupWindow.gIdentityHandler._identityPopup
    );
    popupWindow.gIdentityHandler._identityIconBox.click();
    info("Waiting for the Control Center to be shown");
    await popupShown;
    ok(
      !popupWindow.gIdentityHandler._identityPopup.hidden,
      "Control Center is visible"
    );

    // Validate that the predecessor is shown in the identity panel
    ok(
      popupWindow.gIdentityHandler._identityPopupMainViewHeaderLabel.textContent.includes(
        TEST_HOST
      ),
      "Identity UI header shows the host of the predecessor"
    );

    // Validate that the correct message is displayed
    is(
      popupWindow.gIdentityHandler._identityPopup.getAttribute("connection"),
      "associated",
      "Identity UI shows associated message."
    );

    // Validate that there is no additional security info
    let securityButton = popupWindow.gBrowser.ownerDocument.querySelector(
      "#identity-popup-security-button"
    );
    is(
      securityButton.disabled,
      true,
      "Security button has correct disabled state"
    );

    // Navigate away to a localhost page and make sure the identity icon changes
    let loaded = BrowserTestUtils.browserLoaded(
      popupWindow.gBrowser.selectedBrowser,
      false,
      DUMMY_URI
    );
    await SpecialPowers.spawn(
      popupWindow.gBrowser.selectedBrowser,
      [DUMMY_URI],
      async function (uri) {
        content.location = uri;
      }
    );
    info("Waiting for the navigation to a dummy page to complete.");
    await loaded;

    identityIconImageURL = popupWindow.gBrowser.ownerGlobal
      .getComputedStyle(identityIcon)
      .getPropertyValue("list-style-image");
    is(
      identityIconImageURL,
      `url("chrome://global/skin/icons/page-portrait.svg")`,
      "The identity icon has a correct image url after navigating away."
    );

    // Exit this test, cleaning up as we go.
    await BrowserTestUtils.closeWindow(popupWindow);
  });
}

add_task(async function test_document_write() {
  await web_controlled_about_blank_helper("document_write");
});

add_task(async function test_innerHTML() {
  await web_controlled_about_blank_helper("innerhtml");
});
