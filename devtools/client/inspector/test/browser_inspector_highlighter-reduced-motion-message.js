/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const REDUCED_MOTION_PREF = "ui.prefersReducedMotion";
const DISMISS_MESSAGE_PREF =
  "devtools.inspector.simple-highlighters.message-dismissed";

add_task(async function testMessageHiddenWhenPrefersReducedMotionDisabled() {
  info("Disable ui.prefersReducedMotion");
  await pushPref(REDUCED_MOTION_PREF, 0);

  await pushPref(DISMISS_MESSAGE_PREF, false);

  const tab = await addTab("data:text/html,test");
  const toolbox = await gDevTools.showToolboxForTab(tab);

  await wait(1000);
  ok(
    !getSimpleHighlightersMessage(toolbox),
    "The simple highlighters notification is not displayed"
  );
});

add_task(async function testMessageHiddenWhenAlreadyDismissed() {
  info("Enable ui.prefersReducedMotion");
  await pushPref(REDUCED_MOTION_PREF, 1);

  info("Simulate already dismissed message");
  await pushPref(DISMISS_MESSAGE_PREF, true);

  const tab = await addTab("data:text/html,test");
  const toolbox = await gDevTools.showToolboxForTab(tab);

  await wait(1000);
  ok(
    !getSimpleHighlightersMessage(toolbox),
    "The simple highlighters notification is not displayed"
  );
});

// Check that the message is displayed under the expected conditions, that the
// settings button successfully opens the corresponding panel and that after
// dismissing the message once, it is no longer displayed.
add_task(async function () {
  info("Enable ui.prefersReducedMotion");
  await pushPref(REDUCED_MOTION_PREF, 1);

  info("Simulate already dismissed message");
  await pushPref(DISMISS_MESSAGE_PREF, false);

  const tab = await addTab("data:text/html,test");
  let toolbox = await gDevTools.showToolboxForTab(tab);

  info("Check the simple-highlighters message is displayed");
  let notification = await waitFor(() => getSimpleHighlightersMessage(toolbox));
  ok(notification, "A notification was displayed");

  info("Click on the settings button from the notification");
  const onSettingsCallbackDone = toolbox.once(
    "test-highlighters-settings-opened"
  );
  const settingsButton = notification.querySelector(".notificationButton");
  settingsButton.click();

  info("Wait until the open settings button callback is done");
  await onSettingsCallbackDone;
  is(toolbox.currentToolId, "options", "The options panel was selected");

  info("Close and reopen the toolbox");
  await toolbox.destroy();
  toolbox = await gDevTools.showToolboxForTab(tab);

  info("Check the notification is displayed again");
  notification = await waitFor(() => getSimpleHighlightersMessage(toolbox));
  ok(notification, "A notification was displayed after reopening the toolbox");

  info("Close the notification");
  const closeButton = notification.querySelector(".messageCloseButton");
  closeButton.click();

  info("Wait for the notification to be removed");
  await waitFor(() => !getSimpleHighlightersMessage(toolbox));

  info("Close and reopen the toolbox");
  await toolbox.destroy();
  toolbox = await gDevTools.showToolboxForTab(tab);

  await wait(1000);
  ok(!getSimpleHighlightersMessage(toolbox));
  is(
    Services.prefs.getBoolPref(DISMISS_MESSAGE_PREF),
    true,
    "The dismiss simple-highlighters-message preference was set to true"
  );
});

function getSimpleHighlightersMessage(toolbox) {
  return toolbox.doc.querySelector(
    '.notification[data-key="simple-highlighters-message"]'
  );
}
