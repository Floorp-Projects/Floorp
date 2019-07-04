/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Basic UI tests for the protections panel */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.protections_panel.enabled", true],
      // Set the auto hide timing to 100ms for blocking the test less.
      ["browser.protections_panel.toast.timeout", 100],
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

  // We need to wait toast's popup shown and popup hidden events. It won't fire
  // the popup shown event if we open the protections panel while the toast is
  // opening.
  let popupShownPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popupshown");
  popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");

  await browserLoadedPromise;

  // Wait until the toast is shown and hidden.
  await popupShownPromise;
  await popuphiddenPromise;

  await openProtectionsPanel();
  ok(!gProtectionsHandler._protectionsPopupTPSwitch.hasAttribute("enabled"), "TP Switch should be disabled");
  Services.perms.remove(ContentBlocking._baseURIForChannelClassifier, "trackingprotection");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testSiteNotWorking() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");
  await openProtectionsPanel();
  let viewShownPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopupMultiView, "ViewShown");
  document.getElementById("protections-popup-tp-switch-breakage-link").click();
  let event = await viewShownPromise;
  is(event.originalTarget.id, "protections-popup-siteNotWorkingView", "Site Not Working? view should be shown");
  viewShownPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopupMultiView, "ViewShown");
  document.getElementById("protections-popup-siteNotWorkingView-sendReport").click();
  event = await viewShownPromise;
  is(event.originalTarget.id, "protections-popup-sendReportView", "Send Report view should be shown");
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

/**
 * A test for the 'Show Full Report' button in the footer seciton.
 */
add_task(async function testShowFullReportLink() {
  // Open a tab and its protection panel.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");
  await openProtectionsPanel();

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:protections");
  let showFullReportLink =
    document.getElementById("protections-popup-show-full-report-link");

  showFullReportLink.click();

  // The protection popup should be hidden after clicking the link.
  await popuphiddenPromise;
  // Wait until the 'about:protections' has been opened correctly.
  let newTab = await newTabPromise;

  ok(true, "about:protections has been opened successfully");

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for ensuring the mini panel is working correctly
 */
add_task(async function testMiniPanel() {
  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");

  // Open the mini panel.
  await openProtectionsPanel(true);
  let popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");

  // Check that only the header is displayed.
  for (let item of protectionsPopupMainView.childNodes) {
    if (item.id !== "protections-popup-mainView-panel-header") {
      ok(!BrowserTestUtils.is_visible(item),
         `The section '${item.id}' is hidden in the toast.`);
    } else {
      ok(BrowserTestUtils.is_visible(item),
         "The panel header is displayed as the content of the toast.");
    }
  }

  // Wait until the auto hide is happening.
  await popuphiddenPromise;

  ok(true, "The mini panel hides automatically.");

  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for the toggle switch flow
 */
add_task(async function testToggleSwitchFlow() {
  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");
  await openProtectionsPanel();

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");
  let popupShownPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popupshown");
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Click the TP switch, from On -> Off.
  gProtectionsHandler._protectionsPopupTPSwitch.click();

  // The panel should be closed and the mini panel will show up after refresh.
  await popuphiddenPromise;
  await browserLoadedPromise;
  await popupShownPromise;

  ok(gProtectionsHandler._protectionsPopup.hasAttribute("toast"),
     "The protections popup should have the 'toast' attribute.");

  // Click on the mini panel and making sure the protection popup shows up.
  popupShownPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popupshown");
  popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");
  protectionsPopupHeader.click();
  await popuphiddenPromise;
  await popupShownPromise;

  ok(!gProtectionsHandler._protectionsPopup.hasAttribute("toast"),
     "The 'toast' attribute should be cleared on the protections popup.");

  // Click the TP switch again, from Off -> On.
  popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");
  popupShownPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popupshown");
  browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  gProtectionsHandler._protectionsPopupTPSwitch.click();

  // Protections popup hidden -> Page refresh -> Mini panel shows up.
  await popuphiddenPromise;
  popuphiddenPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popuphidden");
  await browserLoadedPromise;
  await popupShownPromise;

  ok(gProtectionsHandler._protectionsPopup.hasAttribute("toast"),
     "The protections popup should have the 'toast' attribute.");

  // Wait until the auto hide is happening.
  await popuphiddenPromise;

  // Clean up the TP state.
  Services.perms.remove(ContentBlocking._baseURIForChannelClassifier, "trackingprotection");
  BrowserTestUtils.removeTab(tab);
});
