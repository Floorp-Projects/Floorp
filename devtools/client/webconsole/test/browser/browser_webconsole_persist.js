/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that message persistence works - bug 705921 / bug 1307881

"use strict";

const TEST_FILE = "test-console.html";
const TEST_COM_URI = URL_ROOT_COM + TEST_FILE;
const TEST_ORG_URI = URL_ROOT_ORG + TEST_FILE;
const TEST_NET_URI = URL_ROOT_NET + TEST_FILE;

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.webconsole.persistlog");
});

const INITIAL_LOGS_NUMBER = 5;

const { MESSAGE_TYPE } = require("devtools/client/webconsole/constants");

async function logAndAssertInitialMessages(hud) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [INITIAL_LOGS_NUMBER],
    count => {
      content.wrappedJSObject.doLogs(count);
    }
  );
  await waitFor(() => findMessages(hud, "").length === INITIAL_LOGS_NUMBER);
  ok(true, "Messages showed up initially");
}

add_task(async function() {
  info("Testing that messages disappear on a refresh if logs aren't persisted");
  const hud = await openNewTabAndConsole(TEST_COM_URI);

  await logAndAssertInitialMessages(hud);

  const onReloaded = hud.ui.once("reloaded");
  await refreshTab();
  await onReloaded;

  info("Wait for messages to be cleared");
  await waitFor(() => findMessages(hud, "").length === 0);
  ok(true, "Messages disappeared");

  await closeToolbox();
});

add_task(async function() {
  info(
    "Testing that messages disappear on a cross origin navigation if logs aren't persisted"
  );
  const hud = await openNewTabAndConsole(TEST_COM_URI);

  await logAndAssertInitialMessages(hud);

  await navigateTo(TEST_ORG_URI);
  await waitFor(() => findMessages(hud, "").length === 0);
  ok(true, "Messages disappeared");

  await closeToolbox();
});

add_task(async function() {
  info("Testing that messages persist on a refresh if logs are persisted");

  const hud = await openNewTabAndConsole(TEST_COM_URI);

  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-persistentLogs"
  );

  await logAndAssertInitialMessages(hud);

  const onNavigatedMessage = waitForMessage(
    hud,
    "Navigated to " + TEST_COM_URI
  );
  const onReloaded = hud.ui.once("reloaded");
  let timeBeforeNavigation = Date.now();
  refreshTab();
  await onNavigatedMessage;
  await onReloaded;

  ok(true, "Navigation message appeared as expected");
  is(
    findMessages(hud, "").length,
    INITIAL_LOGS_NUMBER + 1,
    "Messages logged before navigation are still visible"
  );

  assertLastMessageIsNavigationMessage(hud, timeBeforeNavigation, TEST_COM_URI);

  info(
    "Testing that messages also persist when doing a cross origin navigation if logs are persisted"
  );
  const onNavigatedMessage2 = waitForMessage(
    hud,
    "Navigated to " + TEST_ORG_URI
  );
  timeBeforeNavigation = Date.now();
  await navigateTo(TEST_ORG_URI);
  await onNavigatedMessage2;

  ok(true, "Second navigation message appeared as expected");
  is(
    findMessages(hud, "").length,
    INITIAL_LOGS_NUMBER + 2,
    "Messages logged before the second navigation are still visible"
  );

  assertLastMessageIsNavigationMessage(hud, timeBeforeNavigation, TEST_ORG_URI);

  info(
    "Test doing a second cross origin navigation in order to triger a target switching with a target following the window global lifecycle"
  );
  const onNavigatedMessage3 = waitForMessage(
    hud,
    "Navigated to " + TEST_NET_URI
  );
  timeBeforeNavigation = Date.now();
  await navigateTo(TEST_NET_URI);
  await onNavigatedMessage3;

  ok(true, "Third navigation message appeared as expected");
  is(
    findMessages(hud, "").length,
    INITIAL_LOGS_NUMBER + 3,
    "Messages logged before the third navigation are still visible"
  );

  assertLastMessageIsNavigationMessage(hud, timeBeforeNavigation, TEST_NET_URI);

  await closeToolbox();
});

function assertLastMessageIsNavigationMessage(hud, timeBeforeNavigation, url) {
  const {
    visibleMessages,
    messagesById,
  } = hud.ui.wrapper.getStore().getState().messages;
  const lastMessageId = visibleMessages.at(-1);
  const lastMessage = messagesById.get(lastMessageId);

  is(
    lastMessage.type,
    MESSAGE_TYPE.NAVIGATION_MARKER,
    "The last message is a navigation marker"
  );
  is(
    lastMessage.messageText,
    "Navigated to " + url,
    "The navigation message is correct"
  );
  // It is surprising, but the navigation may be timestamped at the same exact time
  // as timeBeforeNavigation time record.
  ok(
    lastMessage.timeStamp >= timeBeforeNavigation,
    "The navigation message has a timestamp newer (or equal) than the time before the navigation..."
  );
  ok(lastMessage.timeStamp < Date.now(), "...and older than current time");
}
