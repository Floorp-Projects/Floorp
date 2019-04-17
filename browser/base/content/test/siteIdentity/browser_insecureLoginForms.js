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
    "set": [
      ["security.insecure_password.ui.enabled", true],
      // By default, proxies don't apply to 127.0.0.1. We need them to for this test, though:
      ["network.proxy.allow_hijacking_localhost", true],
    ],
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
    let promisePanelOpen = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    gIdentityHandler._identityBox.click();
    await promisePanelOpen;

    // Messages should be visible when the scheme is HTTP, and invisible when
    // the scheme is HTTPS.
    is(Array.prototype.every.call(document.getElementById("identity-popup-mainView")
                                          .querySelectorAll("[when-loginforms=insecure]"),
                                  element => !BrowserTestUtils.is_hidden(element)),
       expectWarning,
       "The relevant messages should be visible or hidden in the main view.");

    let promiseViewShown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "ViewShown");
    document.getElementById("identity-popup-security-expander").click();
    await promiseViewShown;

    if (expectWarning) {
      ok(BrowserTestUtils.is_visible(document.getElementById("connection-icon")), "Connection icon should be visible");
      let connectionIconImage = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("connection-icon"))
            .getPropertyValue("list-style-image");
      let securityViewBG = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("identity-popup-securityView")
                                      .getElementsByClassName("identity-popup-security-content")[0])
            .getPropertyValue("background-image");
      let securityContentBG = gBrowser.ownerGlobal
            .getComputedStyle(document.getElementById("identity-popup-mainView")
                                      .getElementsByClassName("identity-popup-security-content")[0])
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
      ok(!BrowserTestUtils.is_hidden(document.getElementById("identity-popup-insecure-login-forms-learn-more")),
         "The 'Learn more' link should be visible.");
    }

    // Messages should be visible when the scheme is HTTP, and invisible when
    // the scheme is HTTPS.
    is(Array.prototype.every.call(document.getElementById("identity-popup-securityView")
                                          .querySelectorAll("[when-loginforms=insecure]"),
                                  element => !BrowserTestUtils.is_hidden(element)),
       expectWarning,
       "The relevant messages should be visible or hidden in the security view.");

    if (gIdentityHandler._identityPopup.state != "closed") {
      let hideEvent = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");
      info("hiding popup");
      gIdentityHandler._identityPopup.hidePopup();
      await hideEvent;
    }

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

  await assertMixedContentBlockingState(browser, { activeLoaded: true,
                                                   activeBlocked: false,
                                                   passiveLoaded: false });

  gBrowser.removeTab(tab);
});

/**
 * Checks that insecure window.opener does not trigger a warning.
 */
add_task(async function test_ignoring_window_opener() {
  let path = getRootDirectory(gTestPath)
    .replace("chrome://mochitests/content", "http://example.com");
  let url = path + "insecure_opener.html";

  await BrowserTestUtils.withNewTab(url, async function(browser) {
    // Clicking the link will spawn a new tab.
    let stateChangePromise;
    let tabOpenPromise = new Promise(resolve => {
      gBrowser.tabContainer.addEventListener("TabOpen", event => {
        let tab = event.target;
        let newTabBrowser = tab.linkedBrowser;
        stateChangePromise = waitForInsecureLoginFormsStateChange(newTabBrowser, 2);
        resolve(tab);
      }, { once: true });
    });

    await ContentTask.spawn(browser, {}, function() {
      content.document.getElementById("link").click();
    });
    let tab = await tabOpenPromise;
    await stateChangePromise;

    // Open the identity popup.
    let { gIdentityHandler } = gBrowser.ownerGlobal;
    let promisePanelOpen = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    gIdentityHandler._identityBox.click();
    await promisePanelOpen;

    ok(Array.prototype.every.call(document.getElementById("identity-popup-mainView")
                                          .querySelectorAll("[when-loginforms=insecure]"),
                                  element => BrowserTestUtils.is_hidden(element)),
       "All messages should be hidden in the main view.");

    let promiseViewShown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "ViewShown");
    document.getElementById("identity-popup-security-expander").click();
    await promiseViewShown;

    ok(BrowserTestUtils.is_visible(document.getElementById("connection-icon")),
       "Connection icon is visible");

    // Assert that the identity indicators are still "secure".
    let connectionIconImage = gBrowser.ownerGlobal
          .getComputedStyle(document.getElementById("connection-icon"))
          .getPropertyValue("list-style-image");
    let securityViewBG = gBrowser.ownerGlobal
          .getComputedStyle(document.getElementById("identity-popup-securityView")
                                    .getElementsByClassName("identity-popup-security-content")[0])
          .getPropertyValue("background-image");
    let securityContentBG = gBrowser.ownerGlobal
          .getComputedStyle(document.getElementById("identity-popup-mainView")
                                    .getElementsByClassName("identity-popup-security-content")[0])
          .getPropertyValue("background-image");
    is(connectionIconImage,
       "url(\"chrome://browser/skin/connection-secure.svg\")",
       "Using expected icon image in the identity block");
    is(securityViewBG,
       "url(\"chrome://browser/skin/controlcenter/connection.svg\")",
       "Using expected icon image in the Control Center main view");
    is(securityContentBG,
       "url(\"chrome://browser/skin/controlcenter/connection.svg\")",
       "Using expected icon image in the Control Center subview");

    ok(Array.prototype.every.call(document.getElementById("identity-popup-securityView")
                                          .querySelectorAll("[when-loginforms=insecure]"),
                                  element => BrowserTestUtils.is_hidden(element)),
       "All messages should be hidden in the security view.");

    if (gIdentityHandler._identityPopup.state != "closed") {
      info("hiding popup");
      let hideEvent = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");
      gIdentityHandler._identityPopup.hidePopup();
      await hideEvent;
    }

    BrowserTestUtils.removeTab(tab);
  });
});
