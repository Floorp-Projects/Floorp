/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Basic UI tests for the protections panel */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.protections_panel.enabled", true],
    ],
  });
});

add_task(async function testToggleSwitch() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");
  await openProtectionsPanel();
  ok(gProtectionsHandler._protectionsPopupTPSwitch.hasAttribute("enabled"), "TP Switch should be enabled");
  let popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  gProtectionsHandler._protectionsPopupTPSwitch.click();
  await popuphiddenPromise;
  await browserLoadedPromise;
  await openProtectionsPanel();
  ok(!gProtectionsHandler._protectionsPopupTPSwitch.hasAttribute("enabled"), "TP Switch should be disabled");
  Services.perms.remove(ContentBlocking._baseURIForChannelClassifier, "trackingprotection");
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for the protection settings button.
 */
add_task(async function testSettingsButton() {
  // Open a tab and its protection panel.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");
  await openProtectionsPanel();

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#privacy");
  gProtectionsHandler._protectionPopupSettingsButton.click();

  // The protection popup should be hidden after clicking settings button.
  await popuphiddenPromise;
  // Wait until the about:preferences has been opened correctly.
  let newTab = await newTabPromise;

  ok(true, "about:preferences has been opened successfully");

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});
