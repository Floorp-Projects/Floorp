/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const BENIGN_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";

/**
 * Enable local telemetry recording for the duration of the tests.
 */
var oldCanRecord = Services.telemetry.canRecordExtended;
Services.telemetry.canRecordExtended = true;
registerCleanupFunction(function() {
  Services.telemetry.canRecordExtended = oldCanRecord;
});

add_task(async function testIdentityPopupEvents() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await promiseTabLoadEvent(tab, BENIGN_PAGE);

  Services.telemetry.clearEvents();

  let shown = BrowserTestUtils.waitForEvent(
    gIdentityHandler._identityPopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(gIdentityHandler._identityBox, {});
  await shown;

  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  ).parent;
  let openEvents = events.filter(
    e =>
      e[1] == "security.ui.identitypopup" &&
      e[2] == "open" &&
      e[3] == "identity_popup"
  );
  is(openEvents.length, 1, "recorded telemetry for opening the identity popup");

  gBrowser.removeCurrentTab();
});
