/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const BAD_CERT = "https://expired.example.com/";
const BAD_STS_CERT = "https://badchain.include-subdomains.pinning.example.com:443";

add_task(async function checkTelemetryClickEvents() {
  info("Loading a bad cert page and verifying telemetry click events arrive.");

  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });

  // For obvious reasons event telemetry in the content processes updates with
  // the main processs asynchronously, so we need to wait for the main process
  // to catch up through the entire test.

  // There's an arbitrary interval of 2 seconds in which the content
  // processes sync their event data with the parent process, we wait
  // this out to ensure that we clear everything that is left over from
  // previous tests and don't receive random events in the middle of our tests.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(c => setTimeout(c, 2000));

  // Clear everything.
  Services.telemetry.clearEvents();
  await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true).content;
    return !events || !events.length;
  });

  // Now enable recording our telemetry. Even if this is disabled, content
  // processes will send event telemetry to the parent, thus we needed to ensure
  // we waited and cleared first. Sigh.
  Services.telemetry.setEventRecordingEnabled("security.ui.certerror", true);

  for (let useFrame of [false, true]) {
    let recordedObjects = [
      "advanced_button",
      "learn_more_link",
      "auto_report_cb",
      "error_code_link",
      "clipboard_button_top",
      "clipboard_button_bot",
      "return_button_top",
    ];

    recordedObjects.push("return_button_adv");
    if (!useFrame) {
      recordedObjects.push("exception_button");
    }

    for (let object of recordedObjects) {
      let tab = await openErrorPage(BAD_CERT, useFrame);
      let browser = tab.linkedBrowser;

      let loadEvents = await TestUtils.waitForCondition(() => {
        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true).content;
        if (events && events.length) {
          events = events.filter(e => e[1] == "security.ui.certerror" && e[2] == "load");
          if (events.length == 1 && events[0][5].is_frame == useFrame.toString()) {
            return events;
          }
        }
        return null;
      }, "recorded telemetry for the load");

      is(loadEvents.length, 1, `recorded telemetry for the load testing ${object}, useFrame: ${useFrame}`);

      await ContentTask.spawn(browser, {frame: useFrame, objectId: object}, async function({frame, objectId}) {
        let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;

        await ContentTaskUtils.waitForCondition(() => doc.body.classList.contains("certerror"), "Wait for certerror to be loaded");

        let domElement = doc.querySelector(`[data-telemetry-id='${objectId}']`);
        domElement.click();
      });

      let clickEvents = await TestUtils.waitForCondition(() => {
        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true).content;
        if (events && events.length) {
          events = events.filter(e => e[1] == "security.ui.certerror" && e[2] == "click" && e[3] == object);
          if (events.length == 1 && events[0][5].is_frame == useFrame.toString()) {
            return events;
          }
        }
        return null;
      }, "Has captured telemetry events.");

      is(clickEvents.length, 1, `recorded telemetry for the click on ${object}, useFrame: ${useFrame}`);

      // We opened an extra tab for the SUMO page, need to close it.
      if (object == "learn_more_link") {
        BrowserTestUtils.removeTab(gBrowser.selectedTab);
      }

      if (object == "exception_button") {
        let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                                    .getService(Ci.nsICertOverrideService);
        certOverrideService.clearValidityOverride("expired.example.com", -1);
      }

      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  }

  let enableCertErrorUITelemetry =
    Services.prefs.getBoolPref("security.certerrors.recordEventTelemetry");
  Services.telemetry.setEventRecordingEnabled("security.ui.certerror",
    enableCertErrorUITelemetry);
});
