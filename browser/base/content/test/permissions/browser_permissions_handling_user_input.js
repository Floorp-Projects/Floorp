/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIGIN = "https://example.com";
const PERMISSIONS_PAGE = getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) + "permissions.html";

function assertShown(task) {
  return BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");

    await ContentTask.spawn(browser, null, task);

    await popupshown;

    ok(true, "Notification permission prompt was shown");
  });
}

function assertNotShown(task) {
  return BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");

    await ContentTask.spawn(browser, null, task);

    let sawPrompt = await Promise.race([
      popupshown.then(() => true),
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      new Promise(c => setTimeout(() => c(false), 1000)),
    ]);

    is(sawPrompt, false, "Notification permission prompt was not shown");
  });
}

// Tests that notification permissions are automatically denied without user interaction.
add_task(async function testNotificationPermission() {
  Services.prefs.setBoolPref("dom.webnotifications.requireuserinteraction", true);

  // First test that when user interaction is required, requests
  // with user interaction will show the permission prompt.

  await assertShown(function() {
    E10SUtils.wrapHandlingUserInput(content, true, function() {
      content.document.getElementById("desktop-notification").click();
    });
  });

  await assertShown(function() {
    E10SUtils.wrapHandlingUserInput(content, true, function() {
      content.document.getElementById("push").click();
    });
  });

  // Now test that requests without user interaction will fail.

  await assertNotShown(function() {
    content.postMessage("push", "*");
  });

  await assertNotShown(async function() {
    let response = await content.Notification.requestPermission();
    is(response, "default", "The request was automatically denied");
  });

  Services.prefs.setBoolPref("dom.webnotifications.requireuserinteraction", false);

  // Finally test that those requests will show a prompt again
  // if the pref has been set to false.

  await assertShown(function() {
    content.postMessage("push", "*");
  });

  await assertShown(function() {
    content.Notification.requestPermission();
  });

  Services.prefs.clearUserPref("dom.webnotifications.requireuserinteraction");
});
