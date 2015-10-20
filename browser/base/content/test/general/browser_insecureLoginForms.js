/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load directly from the browser-chrome support files of login tests.
const testUrlPath =
      "://example.com/browser/toolkit/components/passwordmgr/test/browser/";

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
add_task(function* test_simple() {
  for (let scheme of ["http", "https"]) {
    let tab = gBrowser.addTab(scheme + testUrlPath + "form_basic.html");
    let browser = tab.linkedBrowser;
    yield Promise.all([
      BrowserTestUtils.switchTab(gBrowser, tab),
      BrowserTestUtils.browserLoaded(browser),
      // One event is triggered by pageshow and one by DOMFormHasPassword.
      waitForInsecureLoginFormsStateChange(browser, 2),
    ]);

    let { gIdentityHandler } = gBrowser.ownerGlobal;
    gIdentityHandler._identityBox.click();
    document.getElementById("identity-popup-security-expander").click();

    if (scheme == "http") {
      let identityBoxImage = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("page-proxy-favicon"), "")
            .getPropertyValue("list-style-image");
      let securityViewBG = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("identity-popup-securityView"), "")
            .getPropertyValue("background-image");
      let securityContentBG = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("identity-popup-security-content"), "")
            .getPropertyValue("background-image");
      is(identityBoxImage,
         "url(\"chrome://browser/skin/identity-mixed-active-loaded.svg\")",
         "Using expected icon image in the identity block");
      is(securityViewBG,
         "url(\"chrome://browser/skin/controlcenter/mcb-disabled.svg\")",
         "Using expected icon image in the Control Center main view");
      is(securityContentBG,
         "url(\"chrome://browser/skin/controlcenter/mcb-disabled.svg\")",
         "Using expected icon image in the Control Center subview");
    }

    // Messages should be visible when the scheme is HTTP, and invisible when
    // the scheme is HTTPS.
    is(Array.every(document.querySelectorAll("[when-loginforms=insecure]"),
                   element => !is_hidden(element)),
       scheme == "http",
       "The relevant messages should visible or hidden.");

    gIdentityHandler._identityPopup.hidden = true;
    gBrowser.removeTab(tab);
  }
});

/**
 * Checks that the insecure login forms logic does not regress mixed content
 * blocking messages when mixed active content is loaded.
 */
add_task(function* test_mixedcontent() {
  yield new Promise(resolve => SpecialPowers.pushPrefEnv({
    "set": [["security.mixed_content.block_active_content", false]],
  }, resolve));

  // Load the page with the subframe in a new tab.
  let tab = gBrowser.addTab("https" + testUrlPath + "insecure_test.html");
  let browser = tab.linkedBrowser;
  yield Promise.all([
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
