/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function addNotification(box, label, value, priorityName) {
  let added = BrowserTestUtils.waitForNotificationInNotificationBox(box, value);
  let priority = gNotificationBox[`PRIORITY_${priorityName}_MEDIUM`];
  let notification = box.appendNotification(value, { label, priority });
  await added;
  return notification;
}

add_task(async function testStackingOrder() {
  const tabNotificationBox = gBrowser.getNotificationBox();
  ok(
    gNotificationBox.stack.hasAttribute("prepend-notifications"),
    "Browser stack will prepend"
  );
  ok(
    !tabNotificationBox.stack.hasAttribute("prepend-notifications"),
    "Tab stack will append"
  );

  let browserOne = await addNotification(
    gNotificationBox,
    "My first browser notification",
    "browser-one",
    "INFO"
  );

  let tabOne = await addNotification(
    tabNotificationBox,
    "My first tab notification",
    "tab-one",
    "CRITICAL"
  );

  let browserTwo = await addNotification(
    gNotificationBox,
    "My second browser notification",
    "browser-two",
    "CRITICAL"
  );
  let browserThree = await addNotification(
    gNotificationBox,
    "My third browser notification",
    "browser-three",
    "WARNING"
  );

  let tabTwo = await addNotification(
    tabNotificationBox,
    "My second tab notification",
    "tab-two",
    "INFO"
  );
  let tabThree = await addNotification(
    tabNotificationBox,
    "My third tab notification",
    "tab-three",
    "WARNING"
  );

  Assert.deepEqual(
    [browserThree, browserTwo, browserOne],
    [...gNotificationBox.stack.children],
    "Browser notifications prepended"
  );
  Assert.deepEqual(
    [tabOne, tabTwo, tabThree],
    [...tabNotificationBox.stack.children],
    "Tab notifications appended"
  );

  gNotificationBox.removeAllNotifications(true);
  tabNotificationBox.removeAllNotifications(true);
});
