/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 874061: test for how the browser and web consoles display messages coming
// from private windows. See bug for description of expected behavior.

"use strict";

const NON_PRIVATE_MESSAGE = "This is not a private message";
const PRIVATE_MESSAGE = "This is a private message";
const PRIVATE_UNDEFINED_FN = "privateException";
const PRIVATE_EXCEPTION = `${PRIVATE_UNDEFINED_FN} is not defined`;

const NON_PRIVATE_TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Not private";
const PRIVATE_TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html>Test console in private windows
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
  const publicTab = await addTab(NON_PRIVATE_TEST_URI);

  await pushPref("devtools.browsertoolbox.fission", false);
  await testBrowserConsole(publicTab);

  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  await testBrowserConsole(publicTab);
});

async function testBrowserConsole(publicTab) {
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
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

  const onLogMessage = waitForMessageByType(
    hud,
    PRIVATE_MESSAGE,
    ".console-api"
  );
  const onErrorMessage = waitForMessageByType(hud, PRIVATE_EXCEPTION, ".error");
  logPrivateMessages(privateBrowser.selectedBrowser);

  await onLogMessage;
  await onErrorMessage;
  ok(true, "Messages are displayed as expected");

  info("Check that commands executed in private windows aren't put in history");
  const privateCommand = `"command in private window"`;
  await executeAndWaitForResultMessage(hud, privateCommand, "");

  const publicHud = await openConsole(publicTab);
  const historyMessage = await executeAndWaitForMessageByType(
    publicHud,
    ":history",
    "",
    ".simpleTable"
  );

  ok(
    Array.from(
      historyMessage.node.querySelectorAll("tr td:last-of-type")
    ).every(td => td.textContent !== privateCommand),
    "command from private window wasn't added to the history"
  );
  await closeConsole(publicTab);

  info("test cached messages");
  await closeConsole(privateTab);
  info("web console closed");
  hud = await openConsole(privateTab);
  ok(hud, "web console reopened");

  await waitFor(() => findConsoleAPIMessage(hud, PRIVATE_MESSAGE));
  await waitFor(() => findErrorMessage(hud, PRIVATE_EXCEPTION));
  ok(
    true,
    "Messages are still displayed after closing and reopening the console"
  );

  info("Test Browser Console");
  await closeConsole(privateTab);
  info("web console closed");
  hud = await BrowserConsoleManager.toggleBrowserConsole();

  // Add a non-private message to the console.
  const onBrowserConsoleNonPrivateMessage = waitForMessageByType(
    hud,
    NON_PRIVATE_MESSAGE,
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [NON_PRIVATE_MESSAGE], function(
    msg
  ) {
    content.console.log(msg);
  });
  await onBrowserConsoleNonPrivateMessage;

  info(
    "Check that cached messages from private tabs are not displayed in the browser console"
  );
  // We do the check at this moment, after we received the "live" message, so the browser
  // console would have displayed any cached messages by now.
  assertNoPrivateMessages(hud);

  const onBrowserConsolePrivateLogMessage = waitForMessageByType(
    hud,
    PRIVATE_MESSAGE,
    ".console-api"
  );
  const onBrowserConsolePrivateErrorMessage = waitForMessageByType(
    hud,
    PRIVATE_EXCEPTION,
    ".error"
  );
  logPrivateMessages(privateBrowser.selectedBrowser);

  info("Wait for private log message");
  await onBrowserConsolePrivateLogMessage;
  info("Wait for private error message");
  await onBrowserConsolePrivateErrorMessage;
  ok(true, "Messages are displayed as expected");

  info("close the private window and check if private messages are removed");
  const onPrivateMessagesCleared = hud.ui.once("private-messages-cleared");
  privateWindow.BrowserTryToCloseWindow();
  await onPrivateMessagesCleared;

  ok(
    findConsoleAPIMessage(hud, NON_PRIVATE_MESSAGE),
    "non-private messages are still shown after private window closed"
  );
  assertNoPrivateMessages(hud);

  info("close the browser console");
  await safeCloseBrowserConsole();

  info("reopen the browser console");
  hud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(hud, "browser console reopened");

  assertNoPrivateMessages(hud);

  info("close the browser console again");
  await safeCloseBrowserConsole();
}

function logPrivateMessages(browser) {
  SpecialPowers.spawn(browser, [], () => content.wrappedJSObject.logMessages());
}

function assertNoPrivateMessages(hud) {
  is(
    findConsoleAPIMessage(hud, PRIVATE_MESSAGE, ":not(.error)")?.textContent,
    undefined,
    "no console message displayed"
  );
  is(
    findErrorMessage(hud, PRIVATE_EXCEPTION)?.textContent,
    undefined,
    "no exception displayed"
  );
}
