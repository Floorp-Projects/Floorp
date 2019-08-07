/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "TargetFactory",
  "devtools/client/framework/target",
  true
);
loader.lazyRequireGetter(
  this,
  "gDevTools",
  "devtools/client/framework/devtools",
  true
);

/**
 * Displays a notification either at the browser or toolbox level, depending on whether
 * a toolbox is currently open for this tab.
 *
 * @param window
 *        The main browser chrome window.
 * @param tab
 *        The browser tab.
 * @param options
 *        Other options associated with opening.  Currently includes:
 *        - `toolbox`: Whether initiated via toolbox button
 *        - `msg`: String to show in the notification
 *        - `priority`: Priority level for the notification, which affects the icon and
 *                      overall appearance.
 */
async function showNotification(
  window,
  tab,
  { toolboxButton, msg, priority } = {}
) {
  // Default to using the browser's per-tab notification box
  let nbox = window.gBrowser.getNotificationBox(tab.linkedBrowser);

  // If opening was initiated by a toolbox button, check for an open
  // toolbox for the tab.  If one exists, use the toolbox's notification box so that the
  // message is placed closer to the action taken by the user.
  if (toolboxButton) {
    const target = await TargetFactory.forTab(tab);
    const toolbox = gDevTools.getToolbox(target);
    if (toolbox) {
      nbox = toolbox.notificationBox;
    }
  }

  const value = "devtools-responsive";
  if (nbox.getNotificationWithValue(value)) {
    // Notification already displayed
    return;
  }

  if (!priority) {
    priority = nbox.PRIORITY_INFO_MEDIUM;
  }

  nbox.appendNotification(msg, value, null, priority, []);
}

exports.showNotification = showNotification;
