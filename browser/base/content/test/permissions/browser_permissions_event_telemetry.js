/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIGIN = "https://example.com";
const PERMISSIONS_PAGE =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) +
  "permissions.html";

async function showPermissionPrompt(browser) {
  let popupshown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  await ContentTask.spawn(browser, null, function() {
    E10SUtils.wrapHandlingUserInput(content, true, () => {
      // We need to synthesize the click instead of calling .click(),
      // otherwise the document will not correctly register the user gesture.
      let EventUtils = ContentTaskUtils.getEventUtils(content);
      let notificationButton = content.document.getElementById(
        "desktop-notification"
      );
      EventUtils.synthesizeMouseAtCenter(
        notificationButton,
        { isSynthesized: false },
        content
      );
    });
  });

  await popupshown;

  ok(true, "Notification permission prompt was shown");
}

function checkEventTelemetry(method) {
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_ALL_CHANNELS,
    true
  ).parent;
  events = events.filter(
    e =>
      e[1] == "security.ui.permissionprompt" &&
      e[2] == method &&
      e[3] == "notifications"
  );
  is(events.length, 1, "recorded telemetry for showing the prompt");
  ok(
    typeof events[0][4] == "string",
    "recorded a hashed and salted variant of the domain"
  );
  is(events[0][4].length * 4, 256, "hash is a 256 bit string");
  ok(
    !events[0][4].includes("example.com"),
    "we're not including the domain by accident"
  );

  // We assume that even the slowest infra machines are able to show
  // a permission prompt within five minutes.
  const FIVE_MINUTES = 1000 * 60 * 5;

  let timeOnPage = Number(events[0][5].timeOnPage);
  let lastInteraction = Number(events[0][5].lastInteraction);
  ok(
    timeOnPage > 0 && timeOnPage < FIVE_MINUTES,
    `Has recorded time on page (${timeOnPage})`
  );
  is(events[0][5].hasUserInput, "true", "Has recorded user input");
  is(events[0][5].allPermsDenied, "3", "Has recorded total denied permissions");
  is(
    events[0][5].allPermsGranted,
    method == "accept" ? "3" : "2",
    "Has recorded total granted permissions"
  );
  is(
    events[0][5].thisPermDenied,
    "0",
    "Has recorded denied notification permissions"
  );
  is(
    events[0][5].thisPermGranted,
    method == "accept" ? "2" : "1",
    "Has recorded granted notification permissions"
  );
  is(
    events[0][5].docHasUserInput,
    "true",
    "Has recorded user input on document"
  );
  ok(
    lastInteraction > Date.now() - FIVE_MINUTES && lastInteraction < Date.now(),
    `Has recorded last user input time (${lastInteraction})`
  );
}

add_task(async function setup() {
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  Services.prefs.setBoolPref("permissions.eventTelemetry.enabled", true);
  Services.prefs.setBoolPref(
    "permissions.desktop-notification.notNow.enabled",
    true
  );

  // Add some example permissions.
  let uri = Services.io.newURI(PERMISSIONS_PAGE);
  let uri2 = Services.io.newURI("https://example.org");
  let uri3 = Services.io.newURI("http://sub.example.org");
  PermissionTestUtils.add(uri, "geo", Services.perms.ALLOW_ACTION);
  PermissionTestUtils.add(
    uri3,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(uri2, "microphone", Services.perms.DENY_ACTION);
  PermissionTestUtils.add(uri, "camera", Services.perms.DENY_ACTION);
  PermissionTestUtils.add(uri2, "geo", Services.perms.DENY_ACTION);

  registerCleanupFunction(() => {
    Services.perms.removeAll();
    Services.prefs.clearUserPref("permissions.eventTelemetry.enabled");
    Services.prefs.clearUserPref(
      "permissions.desktop-notification.notNow.enabled"
    );
    Services.telemetry.canRecordExtended = oldCanRecord;
  });

  Services.telemetry.clearEvents();
});

add_task(async function testAccept() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    await showPermissionPrompt(browser);

    checkEventTelemetry("show");

    let notification = PopupNotifications.panel.firstElementChild;
    EventUtils.synthesizeMouseAtCenter(notification.button, {});

    checkEventTelemetry("accept");

    Services.telemetry.clearEvents();
    PermissionTestUtils.remove(PERMISSIONS_PAGE, "desktop-notification");
  });
});

add_task(async function testDeny() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    await showPermissionPrompt(browser);

    checkEventTelemetry("show");

    let notification = PopupNotifications.panel.firstElementChild;
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});

    checkEventTelemetry("deny");

    Services.telemetry.clearEvents();
    PermissionTestUtils.remove(PERMISSIONS_PAGE, "desktop-notification");
  });
});

add_task(async function testLeave() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PERMISSIONS_PAGE
  );
  await showPermissionPrompt(tab.linkedBrowser);

  checkEventTelemetry("show");

  let tabClosed = BrowserTestUtils.waitForTabClosing(tab);
  await BrowserTestUtils.removeTab(tab);
  await tabClosed;

  checkEventTelemetry("leave");

  Services.telemetry.clearEvents();
  PermissionTestUtils.remove(PERMISSIONS_PAGE, "desktop-notification");
});
