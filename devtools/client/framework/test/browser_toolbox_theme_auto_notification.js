/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SHOW_THEME_NOTIFICATION_PREF = "devtools.theme.show-auto-theme-info";
const FORCE_THEME_NOTIFICATION_PREF = "devtools.theme.force-auto-theme-info";

// Test the Toolbox Notification displayed for the new "auto" theme.
add_task(async function() {
  // The auto-theme notification is normally only displayed when Firefox uses a
  // dark color scheme. To make testing easier, we expose a preference to bypass
  // this is check.
  await pushPref(FORCE_THEME_NOTIFICATION_PREF, true);

  // Force SHOW_THEME_NOTIFICATION_PREF to true in case a previous test already
  // dismissed the notification.
  await pushPref(SHOW_THEME_NOTIFICATION_PREF, true);

  const tab = await addTab("data:text/html,test");
  let toolbox = await gDevTools.showToolboxForTab(tab);

  info("Check the auto-theme notification is displayed");
  let notification = await waitFor(() => getAutoThemeNotification(toolbox));
  ok(notification, "A notification was displayed");

  info("Click on the settings button from the notification");
  const onSettingsCallbackDone = toolbox.once("test-theme-settings-opened");
  const settingsButton = notification.querySelector(".notificationButton");
  settingsButton.click();

  info("Wait until the open settings button callback is done");
  await onSettingsCallbackDone;
  is(toolbox.currentToolId, "options", "The options panel was selected");

  info("Close and reopen the toolbox");
  await toolbox.destroy();
  toolbox = await gDevTools.showToolboxForTab(tab);

  info("Check the notification is displayed again");
  notification = await waitFor(() => getAutoThemeNotification(toolbox));
  ok(notification, "A notification was displayed after reopening the toolbox");

  info("Close the notification");
  const closeButton = notification.querySelector(".messageCloseButton");
  closeButton.click();

  info("Wait for the notification to be removed");
  await waitFor(() => !getAutoThemeNotification(toolbox));

  info("Close and reopen the toolbox");
  await toolbox.destroy();
  toolbox = await gDevTools.showToolboxForTab(tab);

  await wait(1000);
  ok(!getAutoThemeNotification(toolbox));
  is(
    Services.prefs.getBoolPref(SHOW_THEME_NOTIFICATION_PREF),
    false,
    "The show theme notification preference was set to false"
  );
});

function getAutoThemeNotification(toolbox) {
  return toolbox.doc.querySelector(
    '.notification[data-key="auto-theme-notification"]'
  );
}
