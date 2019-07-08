/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIGIN = "https://example.com";
const PERMISSIONS_PAGE =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) +
  "permissions.html";

function testPostPrompt(task) {
  let uri = Services.io.newURI(PERMISSIONS_PAGE);
  return BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    let icon = document.getElementById("web-notifications-notification-icon");
    ok(
      !BrowserTestUtils.is_visible(icon),
      "notifications icon is not visible at first"
    );

    await ContentTask.spawn(browser, null, task);

    await TestUtils.waitForCondition(
      () => BrowserTestUtils.is_visible(icon),
      "notifications icon is visible"
    );
    ok(
      !PopupNotifications.panel.hasAttribute("panelopen"),
      "only the icon is showing, the panel is not open"
    );

    let popupshown = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );
    icon.click();
    await popupshown;

    ok(true, "Notification permission prompt was shown");

    let notification = PopupNotifications.panel.firstElementChild;
    EventUtils.synthesizeMouseAtCenter(notification.button, {});

    is(
      Services.perms.testPermission(uri, "desktop-notification"),
      Ci.nsIPermissionManager.ALLOW_ACTION,
      "User can override the default deny by using the prompt"
    );

    Services.perms.remove(uri, "desktop-notification");
  });
}

add_task(async function testNotificationPermission() {
  Services.prefs.setBoolPref(
    "dom.webnotifications.requireuserinteraction",
    true
  );
  Services.prefs.setBoolPref(
    "permissions.desktop-notification.postPrompt.enabled",
    true
  );

  Services.prefs.setIntPref(
    "permissions.default.desktop-notification",
    Ci.nsIPermissionManager.DENY_ACTION
  );

  // First test that all requests (even with user interaction) will cause a post-prompt
  // if the global default is "deny".

  await testPostPrompt(function() {
    E10SUtils.wrapHandlingUserInput(content, true, function() {
      content.document.getElementById("desktop-notification").click();
    });
  });

  await testPostPrompt(function() {
    E10SUtils.wrapHandlingUserInput(content, true, function() {
      content.document.getElementById("push").click();
    });
  });

  Services.prefs.clearUserPref("permissions.default.desktop-notification");

  // Now test that requests without user interaction will post-prompt when the
  // user interaction requirement is set.

  await testPostPrompt(function() {
    content.postMessage("push", "*");
  });

  await testPostPrompt(async function() {
    let response = await content.Notification.requestPermission();
    is(response, "default", "The request was automatically denied");
  });

  Services.prefs.clearUserPref("dom.webnotifications.requireuserinteraction");
  Services.prefs.clearUserPref(
    "permissions.desktop-notification.postPrompt.enabled"
  );
});
