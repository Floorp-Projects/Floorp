/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load directly from the browser-chrome support files of login tests.
const TEST_URL_PATH = "/browser/toolkit/components/passwordmgr/test/browser/";

/**
 * Waits for the given number of occurrences of InsecureLoginFormsStateChange
 * on the given browser element.
 */
function waitForInsecureLoginFormsStateChange(browser, count) {
  return BrowserTestUtils.waitForEvent(browser, "InsecureLoginFormsStateChange",
                                       false, () => --count == 0);
}

/**
 * Checks the insecure login forms logic for the identity block.
 */
add_task(async function test_simple() {
  await SpecialPowers.pushPrefEnv({
    "set": [["security.insecure_password.ui.enabled", true]],
  });

  for (let [origin, expectWarning] of [
    ["http://example.com", true],
    ["http://127.0.0.1", false],
    ["https://example.com", false],
  ]) {
    let testUrlPath = origin + TEST_URL_PATH;
    let tab = BrowserTestUtils.addTab(gBrowser, testUrlPath + "form_basic.html");
    let browser = tab.linkedBrowser;
    await Promise.all([
      BrowserTestUtils.switchTab(gBrowser, tab),
      BrowserTestUtils.browserLoaded(browser),
      // One event is triggered by pageshow and one by DOMFormHasPassword.
      waitForInsecureLoginFormsStateChange(browser, 2),
    ]);

    let { gIdentityHandler } = gBrowser.ownerGlobal;
    gIdentityHandler._identityBox.click();
    document.getElementById("identity-popup-security-expander").click();

    if (expectWarning) {
      ok(is_visible(document.getElementById("connection-icon")), "Connection icon should be visible");
      let connectionIconImage = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("connection-icon"))
            .getPropertyValue("list-style-image");
      let securityViewBG = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("identity-popup-securityView"))
            .getPropertyValue("background-image");
      let securityContentBG = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("identity-popup-security-content"))
            .getPropertyValue("background-image");
      is(connectionIconImage,
         "url(\"chrome://browser/skin/connection-mixed-active-loaded.svg\")",
         "Using expected icon image in the identity block");
      is(securityViewBG,
         "url(\"chrome://browser/skin/controlcenter/mcb-disabled.svg\")",
         "Using expected icon image in the Control Center main view");
      is(securityContentBG,
         "url(\"chrome://browser/skin/controlcenter/mcb-disabled.svg\")",
         "Using expected icon image in the Control Center subview");
      is(Array.filter(document.querySelectorAll("[observes=identity-popup-insecure-login-forms-learn-more]"),
                      element => !is_hidden(element)).length, 1,
         "The 'Learn more' link should be visible once.");
    }

    // Messages should be visible when the scheme is HTTP, and invisible when
    // the scheme is HTTPS.
    is(Array.every(document.querySelectorAll("[when-loginforms=insecure]"),
                   element => !is_hidden(element)),
       expectWarning,
       "The relevant messages should be visible or hidden.");

    gIdentityHandler._identityPopup.hidden = true;
    gBrowser.removeTab(tab);
  }
});

/**
 * Checks that the insecure login forms logic does not regress mixed content
 * blocking messages when mixed active content is loaded.
 */
add_task(async function test_mixedcontent() {
  await SpecialPowers.pushPrefEnv({
    "set": [["security.mixed_content.block_active_content", false]],
  });

  // Load the page with the subframe in a new tab.
  let testUrlPath = "://example.com" + TEST_URL_PATH;
  let tab = BrowserTestUtils.addTab(gBrowser, "https" + testUrlPath + "insecure_test.html");
  let browser = tab.linkedBrowser;
  await Promise.all([
    BrowserTestUtils.switchTab(gBrowser, tab),
    BrowserTestUtils.browserLoaded(browser),
    // Two events are triggered by pageshow and one by DOMFormHasPassword.
    waitForInsecureLoginFormsStateChange(browser, 3),
  ]);

  assertMixedContentBlockingState(browser, { activeLoaded: true,
                                             activeBlocked: false,
                                             passiveLoaded: false });

  gBrowser.removeTab(tab);
});

/**
 * Checks that insecure window.opener does not trigger a warning.
 */
add_task(async function test_ignoring_window_opener() {
  let newTabURL = "https://example.com" + TEST_URL_PATH + "form_basic.html";
  let path = getRootDirectory(gTestPath)
    .replace("chrome://mochitests/content", "http://example.com");
  let url = path + "insecure_opener.html";

  await BrowserTestUtils.withNewTab(url, async function(browser) {
    // Clicking the link will spawn a new tab.
    let loaded = BrowserTestUtils.waitForNewTab(gBrowser, newTabURL);
    await ContentTask.spawn(browser, {}, function() {
      content.document.getElementById("link").click();
    });
    let tab = await loaded;
    browser = tab.linkedBrowser;
    await waitForInsecureLoginFormsStateChange(browser, 2);

    // Open the identity popup.
    let { gIdentityHandler } = gBrowser.ownerGlobal;
    gIdentityHandler._identityBox.click();
    document.getElementById("identity-popup-security-expander").click();

    ok(is_visible(document.getElementById("connection-icon")),
       "Connection icon is visible");

    // Assert that the identity indicators are still "secure".
    let connectionIconImage = gBrowser.ownerGlobal
          .getComputedStyle(document.getElementById("connection-icon"))
          .getPropertyValue("list-style-image");
    let securityViewBG = gBrowser.ownerGlobal
          .getComputedStyle(document.getElementById("identity-popup-securityView"))
          .getPropertyValue("background-image");
    let securityContentBG = gBrowser.ownerGlobal
          .getComputedStyle(document.getElementById("identity-popup-security-content"))
          .getPropertyValue("background-image");
    is(connectionIconImage,
       "url(\"chrome://browser/skin/connection-secure.svg\")",
       "Using expected icon image in the identity block");
    is(securityViewBG,
       "url(\"chrome://browser/skin/controlcenter/connection.svg#connection-secure\")",
       "Using expected icon image in the Control Center main view");
    is(securityContentBG,
       "url(\"chrome://browser/skin/controlcenter/connection.svg#connection-secure\")",
       "Using expected icon image in the Control Center subview");

    ok(Array.every(document.querySelectorAll("[when-loginforms=insecure]"),
                   element => is_hidden(element)),
       "All messages should be hidden.");

    gIdentityHandler._identityPopup.hidden = true;

    await BrowserTestUtils.removeTab(tab);
  });
});
