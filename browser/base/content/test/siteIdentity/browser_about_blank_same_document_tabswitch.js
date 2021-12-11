/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.org"
);

const TEST_PAGE = TEST_PATH + "open-self-from-frame.html";

add_task(async function test_identityBlock_inherited_blank() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let identityBox = document.getElementById("identity-box");
    // Ensure we remove the 3rd party storage permission for example.org, or
    // it'll mess up other tests:
    let principal = browser.contentPrincipal;
    registerCleanupFunction(() => {
      Services.perms.removeFromPrincipal(
        principal,
        "3rdPartyStorage^http://example.org"
      );
    });
    is(
      identityBox.className,
      "verifiedDomain",
      "Should indicate a secure site."
    );
    // Open a popup from the web content.
    let popupPromise = BrowserTestUtils.waitForNewWindow();
    await SpecialPowers.spawn(browser, [TEST_PAGE], testPage => {
      content.open(testPage, "_blank", "height=300,width=300");
    });
    // Open a tab back in the main window:
    let popup = await popupPromise;
    info("Opened popup");
    let popupBC = popup.gBrowser.selectedBrowser.browsingContext;
    await TestUtils.waitForCondition(
      () => popupBC.children[0]?.currentWindowGlobal
    );

    info("Got frame contents.");

    let otherTabPromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_PAGE
    );
    info("Clicking button");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "button",
      {},
      popupBC.children[0]
    );
    info("Waiting for tab");
    await otherTabPromise;

    ok(
      gURLBar.value.startsWith("example.org/"),
      "URL bar value should be correct, was " + gURLBar.value
    );
    is(
      identityBox.className,
      "notSecure",
      "Identity box should have been updated."
    );

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await BrowserTestUtils.closeWindow(popup);
  });
});
