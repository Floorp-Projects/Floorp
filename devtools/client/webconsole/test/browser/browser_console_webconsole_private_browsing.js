/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 874061: test for how the browser and web consoles display messages coming
// from private windows. See bug for description of expected behavior.

"use strict";

const NON_PRIVATE_MESSAGE = "This is not a private message";
const PRIVATE_MESSAGE = "This is a private message";
const PRIVATE_UNDEFINED_FN = "privateException";
const PRIVATE_EXCEPTION = `${PRIVATE_UNDEFINED_FN} is not defined`;

const NON_PRIVATE_TEST_URI = "data:text/html;charset=utf8,Not private";
const PRIVATE_TEST_URI = `data:text/html;charset=utf8,Test console in private windows
  <script>
    function logMessages() {
      /* Wrap the exception so we don't throw in ContentTask. */
      setTimeout(() => {
        console.log("${PRIVATE_MESSAGE}");
        ${PRIVATE_UNDEFINED_FN}();
      }, 10);
    }
  </script>`;

add_task(async function() {
  await pushPref("devtools.browserconsole.contentMessages", true);
  await addTab(NON_PRIVATE_TEST_URI);

  const privateWindow = await openNewBrowserWindow({ private: true });
  ok(PrivateBrowsingUtils.isWindowPrivate(privateWindow), "window is private");
  const privateBrowser = privateWindow.gBrowser;
  privateBrowser.selectedTab = BrowserTestUtils.addTab(
    privateBrowser,
    PRIVATE_TEST_URI
  );
  const privateTab = privateBrowser.selectedTab;

  info("private tab opened");
  ok(
    PrivateBrowsingUtils.isBrowserPrivate(privateBrowser.selectedBrowser),
    "tab window is private"
  );

  let hud = await openConsole(privateTab);
  ok(hud, "web console opened");

  const onLogMessage = waitForMessage(hud, PRIVATE_MESSAGE);
  const onErrorMessage = waitForMessage(hud, PRIVATE_EXCEPTION, ".error");
  logPrivateMessages(privateBrowser.selectedBrowser);

  await onLogMessage;
  await onErrorMessage;
  ok(true, "Messages are displayed as expected");

  info("test cached messages");
  await closeConsole(privateTab);
  info("web console closed");
  hud = await openConsole(privateTab);
  ok(hud, "web console reopened");

  await waitFor(() => findMessage(hud, PRIVATE_MESSAGE));
  await waitFor(() => findMessage(hud, PRIVATE_EXCEPTION, ".message.error"));
  ok(
    true,
    "Messages are still displayed after closing and reopening the console"
  );

  info("Test browser console");
  await closeConsole(privateTab);
  info("web console closed");
  hud = await BrowserConsoleManager.toggleBrowserConsole();

  // Make sure that the cached messages from private tabs are not displayed in the
  // browser console.
  assertNoPrivateMessages(hud);

  // Add a non-private message to the console.
  const onBrowserConsoleNonPrivateMessage = waitForMessage(
    hud,
    NON_PRIVATE_MESSAGE
  );
  ContentTask.spawn(gBrowser.selectedBrowser, NON_PRIVATE_MESSAGE, function(
    msg
  ) {
    content.console.log(msg);
  });
  await onBrowserConsoleNonPrivateMessage;

  const onBrowserConsolePrivateLogMessage = waitForMessage(
    hud,
    PRIVATE_MESSAGE
  );
  const onBrowserConsolePrivateErrorMessage = waitForMessage(
    hud,
    PRIVATE_EXCEPTION,
    ".error"
  );
  logPrivateMessages(privateBrowser.selectedBrowser);

  await onBrowserConsolePrivateLogMessage;
  await onBrowserConsolePrivateErrorMessage;
  ok(true, "Messages are displayed as expected");

  info("close the private window and check if private messages are removed");
  const onPrivateMessagesCleared = hud.ui.once("private-messages-cleared");
  privateWindow.BrowserTryToCloseWindow();
  await onPrivateMessagesCleared;

  ok(
    findMessage(hud, NON_PRIVATE_MESSAGE),
    "non-private messages are still shown after private window closed"
  );
  assertNoPrivateMessages(hud);

  info("close the browser console");
  await BrowserConsoleManager.toggleBrowserConsole();

  info("reopen the browser console");
  hud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(hud, "browser console reopened");

  assertNoPrivateMessages(hud);
});

function logPrivateMessages(browser) {
  ContentTask.spawn(browser, null, () => content.wrappedJSObject.logMessages());
}

function assertNoPrivateMessages(hud) {
  is(findMessage(hud, PRIVATE_MESSAGE), null, "no console message displayed");
  is(findMessage(hud, PRIVATE_EXCEPTION), null, "no exception displayed");
}
