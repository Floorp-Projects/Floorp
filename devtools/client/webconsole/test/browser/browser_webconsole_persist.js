/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that message persistence works - bug 705921 / bug 1307881

"use strict";

const TEST_FILE = "test-console.html";
const TEST_COM_URI = URL_ROOT_COM_SSL + TEST_FILE;
const TEST_ORG_URI = URL_ROOT_ORG_SSL + TEST_FILE;
// TEST_MOCHI_URI uses a non standart port and hence
// is not subject to https-first mode
const TEST_MOCHI_URI = URL_ROOT_MOCHI_8888 + TEST_FILE;

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.webconsole.persistlog");
});

const INITIAL_LOGS_NUMBER = 5;

const {
  MESSAGE_TYPE,
} = require("resource://devtools/client/webconsole/constants.js");
const {
  WILL_NAVIGATE_TIME_SHIFT,
} = require("resource://devtools/server/actors/webconsole/listeners/document-events.js");

async function logAndAssertInitialMessages(hud) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [INITIAL_LOGS_NUMBER],
    count => {
      content.wrappedJSObject.doLogs(count);
    }
  );
  await waitFor(() => findAllMessages(hud).length === INITIAL_LOGS_NUMBER);
  ok(true, "Messages showed up initially");
}

add_task(async function () {
  info("Testing that messages disappear on a refresh if logs aren't persisted");
  const hud = await openNewTabAndConsole(TEST_COM_URI);

  await logAndAssertInitialMessages(hud);

  const onReloaded = hud.ui.once("reloaded");
  await reloadBrowser();
  await onReloaded;

  info("Wait for messages to be cleared");
  await waitFor(() => findAllMessages(hud).length === 0);
  ok(true, "Messages disappeared");

  await closeToolbox();
});

add_task(async function () {
  info(
    "Testing that messages disappear on a cross origin navigation if logs aren't persisted"
  );
  const hud = await openNewTabAndConsole(TEST_COM_URI);

  await logAndAssertInitialMessages(hud);

  await navigateTo(TEST_ORG_URI);
  await waitFor(() => findAllMessages(hud).length === 0);
  ok(true, "Messages disappeared");

  await closeToolbox();
});

add_task(async function () {
  info("Testing that messages disappear on bfcache navigations");
  const firstLocation =
    "data:text/html,<!DOCTYPE html><script>console.log('first document load');window.onpageshow=()=>console.log('first document show');</script>";
  const secondLocation =
    "data:text/html,<!DOCTYPE html><script>console.log('second document load');window.onpageshow=()=>console.log('second document show');</script>";
  const hud = await openNewTabAndConsole(firstLocation);

  info("Wait for first page messages");
  // Look into .message-body as the default selector also include the frame,
  // which is the document url, which also include the logged string...
  await waitFor(
    () =>
      findMessagePartsByType(hud, {
        text: "first document load",
        typeSelector: ".console-api",
        partSelector: ".message-body",
      }).length === 1 &&
      findMessagePartsByType(hud, {
        text: "first document show",
        typeSelector: ".console-api",
        partSelector: ".message-body",
      }).length === 1
  );
  const firstPageInnerWindowId =
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.innerWindowId;

  await navigateTo(secondLocation);

  const secondPageInnerWindowId =
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.innerWindowId;
  isnot(
    firstPageInnerWindowId,
    secondPageInnerWindowId,
    "The second page is having a distinct inner window id"
  );
  await waitFor(
    () =>
      findMessagePartsByType(hud, {
        text: "second",
        typeSelector: ".console-api",
        partSelector: ".message-body",
      }).length === 2
  );
  ok("Second page message appeared");
  is(
    findMessagePartsByType(hud, {
      text: "first",
      typeSelector: ".console-api",
      partSelector: ".message-body",
    }).length,
    0,
    "First page message disappeared"
  );

  info("Go back to the first page");
  gBrowser.selectedBrowser.goBack();
  // When going pack, the page isn't reloaded, so that we only get the pageshow event
  await waitFor(
    () =>
      findMessagePartsByType(hud, {
        text: "first document show",
        typeSelector: ".console-api",
        partSelector: ".message-body",
      }).length === 1
  );
  ok("First page message re-appeared");
  is(
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.innerWindowId,
    firstPageInnerWindowId,
    "The first page is really a bfcache navigation, keeping the same WindowGlobal"
  );
  is(
    findMessagePartsByType(hud, {
      text: "second",
      typeSelector: ".console-api",
      partSelector: ".message-body",
    }).length,
    0,
    "Second page message disappeared"
  );

  info("Go forward to the original second page");
  gBrowser.selectedBrowser.goForward();
  await waitFor(
    () =>
      findMessagePartsByType(hud, {
        text: "second document show",
        typeSelector: ".console-api",
        partSelector: ".message-body",
      }).length === 1
  );
  ok("Second page message appeared");
  is(
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.innerWindowId,
    secondPageInnerWindowId,
    "The second page is really a bfcache navigation, keeping the same WindowGlobal"
  );
  is(
    findMessagePartsByType(hud, {
      text: "first",
      typeSelector: ".console-api",
      partSelector: ".message-body",
    }).length,
    0,
    "First page message disappeared"
  );

  await closeToolbox();
});

add_task(async function () {
  info("Testing that messages persist on a refresh if logs are persisted");

  const hud = await openNewTabAndConsole(TEST_COM_URI);

  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-persistentLogs"
  );

  await logAndAssertInitialMessages(hud);

  const onNavigatedMessage = waitForMessageByType(
    hud,
    "Navigated to " + TEST_COM_URI,
    ".navigationMarker"
  );
  const onReloaded = hud.ui.once("reloaded");
  // Because will-navigate DOCUMENT_EVENT timestamp is shifted to workaround some other limitation,
  // the reported time of navigation may actually be slightly off and be older than the real navigation start
  let timeBeforeNavigation = Date.now() - WILL_NAVIGATE_TIME_SHIFT;
  reloadBrowser();
  await onNavigatedMessage;
  await onReloaded;

  ok(true, "Navigation message appeared as expected");
  is(
    findAllMessages(hud).length,
    INITIAL_LOGS_NUMBER + 1,
    "Messages logged before navigation are still visible"
  );

  assertLastMessageIsNavigationMessage(hud, timeBeforeNavigation, TEST_COM_URI);

  info(
    "Testing that messages also persist when doing a cross origin navigation if logs are persisted"
  );
  const onNavigatedMessage2 = waitForMessageByType(
    hud,
    "Navigated to " + TEST_ORG_URI,
    ".navigationMarker"
  );
  timeBeforeNavigation = Date.now() - WILL_NAVIGATE_TIME_SHIFT;
  await navigateTo(TEST_ORG_URI);
  await onNavigatedMessage2;

  ok(true, "Second navigation message appeared as expected");
  is(
    findAllMessages(hud).length,
    INITIAL_LOGS_NUMBER + 2,
    "Messages logged before the second navigation are still visible"
  );

  assertLastMessageIsNavigationMessage(hud, timeBeforeNavigation, TEST_ORG_URI);

  info(
    "Test doing a second cross origin navigation in order to triger a target switching with a target following the window global lifecycle"
  );
  const onNavigatedMessage3 = waitForMessageByType(
    hud,
    "Navigated to " + TEST_MOCHI_URI,
    ".navigationMarker"
  );
  timeBeforeNavigation = Date.now() - WILL_NAVIGATE_TIME_SHIFT;
  await navigateTo(TEST_MOCHI_URI);
  await onNavigatedMessage3;

  ok(true, "Third navigation message appeared as expected");
  is(
    findAllMessages(hud).length,
    INITIAL_LOGS_NUMBER + 3,
    "Messages logged before the third navigation are still visible"
  );

  assertLastMessageIsNavigationMessage(
    hud,
    timeBeforeNavigation,
    TEST_MOCHI_URI
  );

  await closeToolbox();
});

add_task(async function consoleClearPersist() {
  info("Testing that messages persist on console.clear if logs are persisted");

  await pushPref("devtools.webconsole.persistlog", true);
  const hud = await openNewTabAndConsole(TEST_COM_URI);

  await logAndAssertInitialMessages(hud);

  info("Send a console.clear() and another log from the content page");
  const onConsoleClearPrevented = waitForMessageByType(
    hud,
    "console.clear() was prevented",
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.console.clear();
    content.wrappedJSObject.console.log("after clear");
  });

  await waitForMessageByType(hud, "after clear", ".log");
  await onConsoleClearPrevented;
  ok(true, "console.clear was handled by the client");

  ok(
    findAllMessages(hud).length === INITIAL_LOGS_NUMBER + 2,
    "All initial messages are still displayed, with the 2 new ones"
  );

  await closeToolbox();
});

function assertLastMessageIsNavigationMessage(hud, timeBeforeNavigation, url) {
  const { visibleMessages, mutableMessagesById } = hud.ui.wrapper
    .getStore()
    .getState().messages;
  const lastMessageId = visibleMessages.at(-1);
  const lastMessage = mutableMessagesById.get(lastMessageId);

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
