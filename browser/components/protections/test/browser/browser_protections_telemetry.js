/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.database.enabled", true],
      ["browser.contentblocking.report.monitor.enabled", true],
      ["browser.contentblocking.report.lockwise.enabled", true],
      ["browser.contentblocking.report.proxy.enabled", true],
      // Change the endpoints to prevent non-local network connections when landing on the page.
      ["browser.contentblocking.report.monitor.url", ""],
      ["browser.contentblocking.report.monitor.sign_in_url", ""],
      ["browser.contentblocking.report.lockwise.url", ""],
      ["browser.contentblocking.report.social.url", ""],
      ["browser.contentblocking.report.cookie.url", ""],
      ["browser.contentblocking.report.tracker.url", ""],
      ["browser.contentblocking.report.fingerprinter.url", ""],
      ["browser.contentblocking.report.cryptominer.url", ""],
    ],
  });

  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

add_task(async function checkTelemetryLoadEvents() {
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
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  });

  Services.telemetry.setEventRecordingEnabled("security.ui.protections", true);

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  let loadEvents = await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    if (events && events.length) {
      events = events.filter(
        e => e[1] == "security.ui.protections" && e[2] == "show"
      );
      if (events.length == 1) {
        return events;
      }
    }
    return null;
  }, "recorded telemetry for showing the report");

  is(loadEvents.length, 1, `recorded telemetry for showing the report`);
  await reloadTab(tab);
  loadEvents = await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    if (events && events.length) {
      events = events.filter(
        e => e[1] == "security.ui.protections" && e[2] == "close"
      );
      if (events.length == 1) {
        return events;
      }
    }
    return null;
  }, "recorded telemetry for closing the report");

  is(loadEvents.length, 1, `recorded telemetry for closing the report`);

  await BrowserTestUtils.removeTab(tab);
});

function waitForTelemetryEventCount(count) {
  info("waiting for telemetry event count of " + count);
  return TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      false
    ).content;
    info("got " + (events && events.length) + " events");
    if (events && events.length == count) {
      return events;
    }
    return null;
  }, "waiting for telemetry event count of: " + count);
}

add_task(async function checkTelemetryClickEvents() {
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
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  });

  Services.telemetry.setEventRecordingEnabled("security.ui.protections", true);

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    // Show all elements, so we can click on them, even though our user is not logged in.
    let hidden_elements = content.document.querySelectorAll(".hidden");
    for (let el of hidden_elements) {
      el.style.display = "block ";
    }

    const syncLink = await ContentTaskUtils.waitForCondition(() => {
      // Opens an extra tab
      return content.document.getElementById("turn-on-sync");
    }, "syncLink exists");

    syncLink.click();
  });

  let events = await waitForTelemetryEventCount(2);
  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "lw_app_link"
  );
  is(events.length, 1, `recorded telemetry for lw_app_link`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const openAboutLogins = await ContentTaskUtils.waitForCondition(() => {
      // Opens an extra tab
      return content.document.getElementById("open-about-logins-button");
    }, "openAboutLogins exists");

    openAboutLogins.click();
  });

  events = await waitForTelemetryEventCount(3);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "lw_open_button"
  );
  is(events.length, 1, `recorded telemetry for lw_open_button`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const lockwiseAppLink = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("lockwise-inline-link");
    }, "lockwiseAppLink exists");

    lockwiseAppLink.click();
  });

  events = await waitForTelemetryEventCount(4);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "lw_sync_link"
  );
  is(events.length, 1, `recorded telemetry for lw_sync_link`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const lockwiseReportLink = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("lockwise-how-it-works");
    }, "lockwiseReportLink exists");

    lockwiseReportLink.click();
  });

  events = await waitForTelemetryEventCount(5);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "lw_about_link"
  );
  is(events.length, 1, `recorded telemetry for lw_about_link`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    let openLockwise = await ContentTaskUtils.waitForCondition(() => {
      // Opens an extra tab
      return content.document.getElementById("lockwise-link");
    }, "openLockwise exists");

    openLockwise.click();
  });

  events = await waitForTelemetryEventCount(6);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "lw_open_breach_link"
  );
  is(events.length, 1, `recorded telemetry for lw_open_breach_link`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    let monitorReportLink = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("monitor-inline-link");
    }, "monitorReportLink exists");

    monitorReportLink.click();
  });

  events = await waitForTelemetryEventCount(7);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "mtr_report_link"
  );
  is(events.length, 1, `recorded telemetry for mtr_report_link`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    let monitorAboutLink = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("monitor-link");
    }, "monitorAboutLink exists");

    monitorAboutLink.click();
  });

  events = await waitForTelemetryEventCount(8);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "mtr_about_link"
  );
  is(events.length, 1, `recorded telemetry for mtr_about_link`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const signUpForMonitorLink = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("sign-up-for-monitor-link");
    }, "signUpForMonitorLink exists");

    signUpForMonitorLink.click();
  });

  events = await waitForTelemetryEventCount(9);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "mtr_signup_button"
  );
  is(events.length, 1, `recorded telemetry for mtr_signup_button`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const socialLearnMoreLink = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("social-link");
    }, "Learn more link for social tab exists");

    socialLearnMoreLink.click();
  });

  events = await waitForTelemetryEventCount(10);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "trackers_about_link" &&
      e[4] == "social"
  );
  is(events.length, 1, `recorded telemetry for social trackers_about_link`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const cookieLearnMoreLink = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("cookie-link");
    }, "Learn more link for cookie tab exists");

    cookieLearnMoreLink.click();
  });

  events = await waitForTelemetryEventCount(11);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "trackers_about_link" &&
      e[4] == "cookie"
  );
  is(events.length, 1, `recorded telemetry for cookie trackers_about_link`);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const trackerLearnMoreLink = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("tracker-link");
    }, "Learn more link for tracker tab exists");

    trackerLearnMoreLink.click();
  });

  events = await waitForTelemetryEventCount(12);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "trackers_about_link" &&
      e[4] == "tracker"
  );
  is(
    events.length,
    1,
    `recorded telemetry for content tracker trackers_about_link`
  );

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const fingerprinterLearnMoreLink = await ContentTaskUtils.waitForCondition(
      () => {
        return content.document.getElementById("fingerprinter-link");
      },
      "Learn more link for fingerprinter tab exists"
    );

    fingerprinterLearnMoreLink.click();
  });

  events = await waitForTelemetryEventCount(13);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "trackers_about_link" &&
      e[4] == "fingerprinter"
  );
  is(
    events.length,
    1,
    `recorded telemetry for fingerprinter trackers_about_link`
  );

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const cryptominerLearnMoreLink = await ContentTaskUtils.waitForCondition(
      () => {
        return content.document.getElementById("cryptominer-link");
      },
      "Learn more link for cryptominer tab exists"
    );

    cryptominerLearnMoreLink.click();
  });

  events = await waitForTelemetryEventCount(14);

  events = events.filter(
    e =>
      e[1] == "security.ui.protections" &&
      e[2] == "click" &&
      e[3] == "trackers_about_link" &&
      e[4] == "cryptominer"
  );
  is(
    events.length,
    1,
    `recorded telemetry for cryptominer trackers_about_link`
  );

  await BrowserTestUtils.removeTab(tab);
  // We open three extra tabs with the click events.
  // 1. Monitor's "Viewed Saved Logins" link (goes to about:logins)
  // 2. Lockwise's "Turn on sync..." link (goes to about:preferences#sync)
  // 3. Lockwise's "Open Nightly" button (goes to about:logins)
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
