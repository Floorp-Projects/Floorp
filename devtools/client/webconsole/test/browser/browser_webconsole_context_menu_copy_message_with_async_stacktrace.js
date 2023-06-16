/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the "Copy message" menu item copies the expected text to the clipboard
// for a message with a stacktrace containing async separators.

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerPathHandler(`/`, function (request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`<script type="text/javascript" src="test.js"></script>`);
});

httpServer.registerPathHandler("/test.js", function (_, response) {
  response.setHeader("Content-Type", "application/javascript");
  response.write(`
    function resolveLater() {
      return new Promise(function p(resolve) {
        setTimeout(function timeout() {
          Promise.resolve("blurp").then(function pthen(){
            console.trace("thenTrace");
            resolve();
          })
        }, 1);
      });
    }

    async function waitForData() {
      await resolveLater();
    }
  `);
});

const TEST_URI = `http://localhost:${httpServer.identity.primaryPort}/`;

add_task(async function () {
  await pushPref("javascript.options.asyncstack_capture_debuggee_only", false);
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Call the log function defined in the test page");
  const onMessage = waitForMessageByType(hud, "thenTrace", ".console-api");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.waitForData();
  });
  const message = await onMessage;
  const messageEl = message.node;
  await waitFor(() => messageEl.querySelector(".frames"));

  const clipboardText = await copyMessageContent(hud, messageEl);
  ok(true, "Clipboard text was found and saved");

  const newLineString = "\n";
  info("Check copied text for the console.trace message");
  const lines = clipboardText.split(newLineString);

  is(
    JSON.stringify(lines, null, 2),
    JSON.stringify(
      [
        `console.trace() thenTrace test.js:6:21`,
        `    pthen ${TEST_URI}test.js:6`,
        `    (Async: promise callback)`,
        `    timeout ${TEST_URI}test.js:5`,
        `    (Async: setTimeout handler)`,
        `    p ${TEST_URI}test.js:4`,
        `    resolveLater ${TEST_URI}test.js:3`,
        `    waitForData ${TEST_URI}test.js:14`,
        ``,
      ],
      null,
      2
    ),
    "Stacktrace was copied as expected"
  );
});

/**
 * Simple helper method to open the context menu on a given message, and click on the copy
 * menu item.
 */
async function copyMessageContent(hud, messageEl) {
  const menuPopup = await openContextMenu(hud, messageEl);
  const copyMenuItem = menuPopup.querySelector("#console-menu-copy");
  ok(copyMenuItem, "copy menu item is enabled");

  return waitForClipboardPromise(
    () => copyMenuItem.click(),
    data => data
  );
}
