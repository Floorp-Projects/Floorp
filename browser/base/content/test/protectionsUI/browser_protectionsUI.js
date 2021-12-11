/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Basic UI tests for the protections panel */

"use strict";

const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";

ChromeUtils.defineModuleGetter(
  this,
  "ContentBlockingAllowList",
  "resource://gre/modules/ContentBlockingAllowList.jsm"
);

const { CustomizableUITestUtils } = ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm"
);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set the auto hide timing to 100ms for blocking the test less.
      ["browser.protections_panel.toast.timeout", 100],
      // Hide protections cards so as not to trigger more async messaging
      // when landing on the page.
      ["browser.contentblocking.report.monitor.enabled", false],
      ["browser.contentblocking.report.lockwise.enabled", false],
      ["browser.contentblocking.report.proxy.enabled", false],
      ["privacy.trackingprotection.enabled", true],
    ],
  });
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.telemetry.clearEvents();

  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.telemetry.clearEvents();
  });
});

add_task(async function testToggleSwitch() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TRACKING_PAGE
  );

  await openProtectionsPanel();

  await TestUtils.waitForCondition(() => {
    return gProtectionsHandler._protectionsPopup.hasAttribute("blocking");
  });

  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS
  ).parent;
  let buttonEvents = events.filter(
    e =>
      e[1] == "security.ui.protectionspopup" &&
      e[2] == "open" &&
      e[3] == "protections_popup"
  );
  is(buttonEvents.length, 1, "recorded telemetry for opening the popup");

  // Check the visibility of the "Site not working?" link.
  ok(
    BrowserTestUtils.is_visible(
      gProtectionsHandler._protectionsPopupTPSwitchBreakageLink
    ),
    "The 'Site not working?' link should be visible."
  );

  // The 'Site Fixed?' link should be hidden.
  ok(
    BrowserTestUtils.is_hidden(
      gProtectionsHandler._protectionsPopupTPSwitchBreakageFixedLink
    ),
    "The 'Site Fixed?' link should be hidden."
  );

  // Navigate through the 'Site Not Working?' flow and back to the main view,
  // checking for telemetry on the way.
  let siteNotWorkingView = document.getElementById(
    "protections-popup-siteNotWorkingView"
  );
  let viewShown = BrowserTestUtils.waitForEvent(
    siteNotWorkingView,
    "ViewShown"
  );
  gProtectionsHandler._protectionsPopupTPSwitchBreakageLink.click();
  await viewShown;

  checkClickTelemetry("sitenotworking_link");

  let sendReportButton = document.getElementById(
    "protections-popup-siteNotWorkingView-sendReport"
  );
  let sendReportView = document.getElementById(
    "protections-popup-sendReportView"
  );
  viewShown = BrowserTestUtils.waitForEvent(sendReportView, "ViewShown");
  sendReportButton.click();
  await viewShown;

  checkClickTelemetry("send_report_link");

  viewShown = BrowserTestUtils.waitForEvent(siteNotWorkingView, "ViewShown");
  sendReportView.querySelector(".subviewbutton-back").click();
  await viewShown;

  let mainView = document.getElementById("protections-popup-mainView");

  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  siteNotWorkingView.querySelector(".subviewbutton-back").click();
  await viewShown;

  ok(
    gProtectionsHandler._protectionsPopupTPSwitch.hasAttribute("enabled"),
    "TP Switch should be enabled"
  );
  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  gProtectionsHandler._protectionsPopupTPSwitch.click();

  // The 'Site not working?' link should be hidden after clicking the TP switch.
  ok(
    BrowserTestUtils.is_hidden(
      gProtectionsHandler._protectionsPopupTPSwitchBreakageLink
    ),
    "The 'Site not working?' link should be hidden after TP switch turns to off."
  );
  // Same for the 'Site Fixed?' link
  ok(
    BrowserTestUtils.is_hidden(
      gProtectionsHandler._protectionsPopupTPSwitchBreakageFixedLink
    ),
    "The 'Site Fixed?' link should be hidden."
  );

  await popuphiddenPromise;
  checkClickTelemetry("etp_toggle_off");

  // We need to wait toast's popup shown and popup hidden events. It won't fire
  // the popup shown event if we open the protections panel while the toast is
  // opening.
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popupshown"
  );
  popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );

  await browserLoadedPromise;

  // Wait until the toast is shown and hidden.
  await popupShownPromise;
  await popuphiddenPromise;

  await openProtectionsPanel();
  ok(
    !gProtectionsHandler._protectionsPopupTPSwitch.hasAttribute("enabled"),
    "TP Switch should be disabled"
  );

  // The 'Site not working?' link should be hidden if the TP is off.
  ok(
    BrowserTestUtils.is_hidden(
      gProtectionsHandler._protectionsPopupTPSwitchBreakageLink
    ),
    "The 'Site not working?' link should be hidden if TP is off."
  );

  // The 'Site Fixed?' link should be shown if TP is off.
  ok(
    BrowserTestUtils.is_visible(
      gProtectionsHandler._protectionsPopupTPSwitchBreakageFixedLink
    ),
    "The 'Site Fixed?' link should be visible."
  );

  // Check telemetry for 'Site Fixed?' link.
  viewShown = BrowserTestUtils.waitForEvent(sendReportView, "ViewShown");
  gProtectionsHandler._protectionsPopupTPSwitchBreakageFixedLink.click();
  await viewShown;

  checkClickTelemetry("sitenotworking_link", "sitefixed");

  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  sendReportView.querySelector(".subviewbutton-back").click();
  await viewShown;

  // Click the TP switch again and check the visibility of the 'Site not
  // Working?'. It should be hidden after toggling the TP switch.
  browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  gProtectionsHandler._protectionsPopupTPSwitch.click();

  ok(
    BrowserTestUtils.is_hidden(
      gProtectionsHandler._protectionsPopupTPSwitchBreakageLink
    ),
    `The 'Site not working?' link should be still hidden after toggling TP
     switch to on from off.`
  );
  ok(
    BrowserTestUtils.is_hidden(
      gProtectionsHandler._protectionsPopupTPSwitchBreakageFixedLink
    ),
    "The 'Site Fixed?' link should be hidden."
  );

  await browserLoadedPromise;
  checkClickTelemetry("etp_toggle_on");

  ContentBlockingAllowList.remove(tab.linkedBrowser);
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for the protection settings button.
 */
add_task(async function testSettingsButton() {
  // Open a tab and its protection panel.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  await openProtectionsPanel();

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:preferences#privacy"
  );
  gProtectionsHandler._protectionsPopupSettingsButton.click();

  // The protection popup should be hidden after clicking settings button.
  await popuphiddenPromise;
  // Wait until the about:preferences has been opened correctly.
  let newTab = await newTabPromise;

  ok(true, "about:preferences has been opened successfully");
  checkClickTelemetry("settings");

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for ensuring Tracking Protection label is shown correctly
 */
add_task(async function testTrackingProtectionLabel() {
  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  await openProtectionsPanel();

  let trackingProtectionLabel = document.getElementById(
    "protections-popup-footer-protection-type-label"
  );

  is(
    trackingProtectionLabel.textContent,
    "Custom",
    "The label is correctly set to Custom."
  );
  await closeProtectionsPanel();

  Services.prefs.setStringPref("browser.contentblocking.category", "standard");
  await openProtectionsPanel();

  is(
    trackingProtectionLabel.textContent,
    "Standard",
    "The label is correctly set to Standard."
  );
  await closeProtectionsPanel();

  Services.prefs.setStringPref("browser.contentblocking.category", "strict");
  await openProtectionsPanel();
  is(
    trackingProtectionLabel.textContent,
    "Strict",
    "The label is correctly set to Strict."
  );

  await closeProtectionsPanel();
  Services.prefs.setStringPref("browser.contentblocking.category", "custom");
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for the 'Show Full Report' button in the footer section.
 */
add_task(async function testShowFullReportButton() {
  // Open a tab and its protection panel.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  await openProtectionsPanel();

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  let newTabPromise = waitForAboutProtectionsTab();
  let showFullReportButton = document.getElementById(
    "protections-popup-show-report-button"
  );

  showFullReportButton.click();

  // The protection popup should be hidden after clicking the link.
  await popuphiddenPromise;
  // Wait until the 'about:protections' has been opened correctly.
  let newTab = await newTabPromise;

  ok(true, "about:protections has been opened successfully");

  checkClickTelemetry("full_report");

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for ensuring the mini panel is working correctly
 */
add_task(async function testMiniPanel() {
  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  // Open the mini panel.
  await openProtectionsPanel(true);
  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );

  // Check that only the header is displayed.
  let mainView = document.getElementById("protections-popup-mainView");
  for (let item of mainView.childNodes) {
    if (item.id !== "protections-popup-mainView-panel-header-section") {
      ok(
        !BrowserTestUtils.is_visible(item),
        `The section '${item.id}' is hidden in the toast.`
      );
    } else {
      ok(
        BrowserTestUtils.is_visible(item),
        "The panel header is displayed as the content of the toast."
      );
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
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  await openProtectionsPanel();

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popupshown"
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Click the TP switch, from On -> Off.
  gProtectionsHandler._protectionsPopupTPSwitch.click();

  // Check that the icon state has been changed.
  ok(
    gProtectionsHandler.iconBox.hasAttribute("hasException"),
    "The tracking protection icon state has been changed to disabled."
  );

  // The panel should be closed and the mini panel will show up after refresh.
  await popuphiddenPromise;
  await browserLoadedPromise;
  await popupShownPromise;

  ok(
    gProtectionsHandler._protectionsPopup.hasAttribute("toast"),
    "The protections popup should have the 'toast' attribute."
  );

  // Click on the mini panel and making sure the protection popup shows up.
  popupShownPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popupshown"
  );
  popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  document.getElementById("protections-popup-mainView-panel-header").click();
  await popuphiddenPromise;
  await popupShownPromise;

  ok(
    !gProtectionsHandler._protectionsPopup.hasAttribute("toast"),
    "The 'toast' attribute should be cleared on the protections popup."
  );

  // Click the TP switch again, from Off -> On.
  popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  popupShownPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popupshown"
  );
  browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  gProtectionsHandler._protectionsPopupTPSwitch.click();

  // Check that the icon state has been changed.
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("hasException"),
    "The tracking protection icon state has been changed to enabled."
  );

  // Protections popup hidden -> Page refresh -> Mini panel shows up.
  await popuphiddenPromise;
  popuphiddenPromise = BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  await browserLoadedPromise;
  await popupShownPromise;

  ok(
    gProtectionsHandler._protectionsPopup.hasAttribute("toast"),
    "The protections popup should have the 'toast' attribute."
  );

  // Wait until the auto hide is happening.
  await popuphiddenPromise;

  // Clean up the TP state.
  ContentBlockingAllowList.remove(tab.linkedBrowser);
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for ensuring the tracking protection icon will show a correct
 * icon according to the TP enabling state.
 */
add_task(async function testTrackingProtectionIcon() {
  // Open a tab and its protection panel.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  let TPIcon = document.getElementById("tracking-protection-icon");
  // Check the icon url. It will show a shield icon if TP is enabled.
  is(
    gBrowser.ownerGlobal
      .getComputedStyle(TPIcon)
      .getPropertyValue("list-style-image"),
    `url("chrome://browser/skin/tracking-protection.svg")`,
    "The tracking protection icon shows a shield icon."
  );

  // Disable the tracking protection.
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    "https://example.com/"
  );
  gProtectionsHandler.disableForCurrentPage();
  await browserLoadedPromise;

  // Check that the tracking protection icon should show a strike-through shield
  // icon after page is reloaded.
  is(
    gBrowser.ownerGlobal
      .getComputedStyle(TPIcon)
      .getPropertyValue("list-style-image"),
    `url("chrome://browser/skin/tracking-protection-disabled.svg")`,
    "The tracking protection icon shows a strike through shield icon."
  );

  // Clean up the TP state.
  ContentBlockingAllowList.remove(tab.linkedBrowser);
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test for ensuring the number of blocked trackers is displayed properly.
 */
add_task(async function testNumberOfBlockedTrackers() {
  // First, clear the tracking database.
  await TrackingDBService.clearAll();

  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  await openProtectionsPanel();

  let trackerCounterBox = document.getElementById(
    "protections-popup-trackers-blocked-counter-box"
  );
  let trackerCounterDesc = document.getElementById(
    "protections-popup-trackers-blocked-counter-description"
  );

  // Check that whether the counter is not shown if the number of blocked
  // trackers is zero.
  ok(
    BrowserTestUtils.is_hidden(trackerCounterBox),
    "The blocked tracker counter is hidden if there is no blocked tracker."
  );

  await closeProtectionsPanel();

  // Add one tracker into the database and check that the tracker counter is
  // properly shown.
  await addTrackerDataIntoDB(1);

  // A promise for waiting the `showing` attributes has been set to the counter
  // box. This means the database access is finished.
  let counterShownPromise = BrowserTestUtils.waitForAttribute(
    "showing",
    trackerCounterBox
  );

  await openProtectionsPanel();
  await counterShownPromise;

  // Check that the number of blocked trackers is shown.
  ok(
    BrowserTestUtils.is_visible(trackerCounterBox),
    "The blocked tracker counter is shown if there is one blocked tracker."
  );
  is(
    trackerCounterDesc.textContent,
    "1 Blocked",
    "The blocked tracker counter is correct."
  );

  await closeProtectionsPanel();
  await TrackingDBService.clearAll();

  // Add trackers into the database and check that the tracker counter is
  // properly shown as well as whether the pre-fetch is triggered by the
  // keyboard navigation.
  await addTrackerDataIntoDB(10);

  // We cannot wait for the change of "showing" attribute here since this
  // attribute will only be set if the previous counter is zero. Instead, we
  // wait for the change of the text content of the counter.
  let updateCounterPromise = new Promise(resolve => {
    let mut = new MutationObserver(mutations => {
      resolve();
      mut.disconnect();
    });

    mut.observe(trackerCounterDesc, {
      childList: true,
    });
  });

  await openProtectionsPanelWithKeyNav();
  await updateCounterPromise;

  // Check that the number of blocked trackers is shown.
  ok(
    BrowserTestUtils.is_visible(trackerCounterBox),
    "The blocked tracker counter is shown if there are more than one blocked tracker."
  );
  is(
    trackerCounterDesc.textContent,
    "10 Blocked",
    "The blocked tracker counter is correct."
  );

  await closeProtectionsPanel();
  await TrackingDBService.clearAll();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testSubViewTelemetry() {
  let items = [
    ["protections-popup-category-trackers", "trackers"],
    ["protections-popup-category-socialblock", "social"],
    ["protections-popup-category-cookies", "cookies"],
    ["protections-popup-category-cryptominers", "cryptominers"],
    ["protections-popup-category-fingerprinters", "fingerprinters"],
  ].map(item => [document.getElementById(item[0]), item[1]]);

  for (let [item, telemetryId] of items) {
    await BrowserTestUtils.withNewTab("http://www.example.com", async () => {
      await openProtectionsPanel();

      item.classList.remove("notFound"); // Force visible for test
      gProtectionsHandler._categoryItemOrderInvalidated = true;
      gProtectionsHandler.reorderCategoryItems();

      let viewShownEvent = BrowserTestUtils.waitForEvent(
        gProtectionsHandler._protectionsPopupMultiView,
        "ViewShown"
      );
      item.click();
      let panelView = (await viewShownEvent).originalTarget;
      checkClickTelemetry(telemetryId);
      let prefsTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        "about:preferences#privacy"
      );
      panelView.querySelector(".panel-subview-footer-button").click();
      let prefsTab = await prefsTabPromise;
      BrowserTestUtils.removeTab(prefsTab);
      checkClickTelemetry("subview_settings", telemetryId);
    });
  }
});

/**
 * A test to make sure the TP state won't apply incorrectly if we quickly switch
 * tab after toggling the TP switch.
 */
add_task(async function testQuickSwitchTabAfterTogglingTPSwitch() {
  const FIRST_TEST_SITE = "https://example.com/";
  const SECOND_TEST_SITE = "https://example.org/";

  // First, clear the tracking database.
  await TrackingDBService.clearAll();

  // Open two tabs with different origins.
  let tabOne = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    FIRST_TEST_SITE
  );
  let tabTwo = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SECOND_TEST_SITE
  );

  // Open the protection panel of the second tab.
  await openProtectionsPanel();

  // A promise to check the reload happens on the second tab.
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tabTwo.linkedBrowser,
    false,
    SECOND_TEST_SITE
  );

  // Toggle the TP state and switch tab without waiting it to be finished.
  gProtectionsHandler._protectionsPopupTPSwitch.click();
  gBrowser.selectedTab = tabOne;

  // Wait for the second tab to be reloaded.
  await browserLoadedPromise;

  // Check that the first tab is still with ETP enabled.
  ok(
    !ContentBlockingAllowList.includes(gBrowser.selectedBrowser),
    "The ETP state of the first tab is still enabled."
  );

  // Check the ETP is disabled on the second origin.
  ok(
    ContentBlockingAllowList.includes(tabTwo.linkedBrowser),
    "The ETP state of the second tab has been changed to disabled."
  );

  // Clean up the state of the allow list for the second tab.
  ContentBlockingAllowList.remove(tabTwo.linkedBrowser);

  BrowserTestUtils.removeTab(tabOne);
  BrowserTestUtils.removeTab(tabTwo);

  // Finally, clear the tracking database.
  await TrackingDBService.clearAll();
});
