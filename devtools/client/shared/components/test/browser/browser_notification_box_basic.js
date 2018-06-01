/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../../shared/test/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js", this);

const TEST_URI = "data:text/html;charset=utf-8,Test page";

/**
 * Basic test that checks existence of the Notification box.
 */
add_task(async function() {
  info("Test Notification box basic started");

  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");

  // Append a notification
  const notificationBox = toolbox.getNotificationBox();
  notificationBox.appendNotification(
    "Info message",
    "id1",
    null,
    notificationBox.PRIORITY_INFO_HIGH
  );

  // Verify existence of one notification.
  const parentNode = toolbox.doc.getElementById("toolbox-notificationbox");
  const nodes = parentNode.querySelectorAll(".notification");
  is(nodes.length, 1, "There must be one notification");
});
