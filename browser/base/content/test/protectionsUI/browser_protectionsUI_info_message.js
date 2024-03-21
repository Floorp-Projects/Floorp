/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests the info message that apears in the protections panel
 * on first render, and afterward by clicking the "info" icon */

"use strict";

const TRACKING_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";

add_setup(async function () {
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
      // Set the infomessage pref to ensure the message is displayed
      // every time
      ["browser.protections_panel.infoMessage.seen", false],
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

add_task(async function testPanelInfoMessage() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TRACKING_PAGE
  );

  await openProtectionsPanel();

  await TestUtils.waitForCondition(() => {
    return gProtectionsHandler._protectionsPopup.hasAttribute(
      "infoMessageShowing"
    );
  });

  // Test that the info message is displayed when the panel opens
  let container = document.getElementById("info-message-container");
  let message = document.getElementById("protections-popup-message");
  let learnMoreLink = document.querySelector(
    "#info-message-container .text-link"
  );

  // Check the visibility of the info message.
  ok(
    BrowserTestUtils.isVisible(container),
    "The message container should exist."
  );

  ok(BrowserTestUtils.isVisible(message), "The message should be visible.");

  ok(BrowserTestUtils.isVisible(learnMoreLink), "The link should be visible.");

  // Check telemetry for the info message
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  ).parent;
  let messageEvents = events.filter(
    e =>
      e[1] == "security.ui.protectionspopup" &&
      e[2] == "open" &&
      e[3] == "protectionspopup_cfr" &&
      e[4] == "impression"
  );
  is(
    messageEvents.length,
    1,
    "recorded telemetry for showing the info message"
  );
  //Clear telemetry from this test so that the next one doesn't fall over
  Services.telemetry.clearEvents();
  BrowserTestUtils.removeTab(tab);
});
