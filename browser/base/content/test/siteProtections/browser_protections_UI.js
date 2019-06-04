/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Basic UI tests for the protections panel */

add_task(async function testToggleSwitch() {
  let uri = "http://example.com";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);
  await openProtectionsPanel();
  ok(gProtectionsHandler._protectionsPopupTPSwitch.hasAttribute("enabled"), "TP Switch should be enabled");
  let popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  gProtectionsHandler._protectionsPopupTPSwitch.click();
  await popuphiddenPromise;
  await browserLoadedPromise;
  await openProtectionsPanel();
  ok(!gProtectionsHandler._protectionsPopupTPSwitch.hasAttribute("enabled"), "TP Switch should be disabled");
  BrowserTestUtils.removeTab(tab);
  Services.perms.remove(uri, "trackingprotection");
});
