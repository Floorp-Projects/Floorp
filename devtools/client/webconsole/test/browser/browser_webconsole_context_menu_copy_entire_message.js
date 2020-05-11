/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerPathHandler(`/`, function(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`
    <meta charset=utf8>
    <h1>Test "copy message" context menu entry</h1>
    <script type="text/javascript" src="test.js"></script>`);
});

httpServer.registerPathHandler("/test.js", function(request, response) {
  response.setHeader("Content-Type", "application/javascript");
  response.write(`
    window.logStuff = function() {
      console.log("simple text message");
      function wrapper() {
        console.log(new Error("error object"));
        console.trace();
        for (let i = 0; i < 2; i++) console.log("repeated")
      }
      wrapper();
    };
    z.bar = "baz";
  `);
});

const TEST_URI = `http://localhost:${httpServer.identity.primaryPort}/`;

// RegExp that validates copied text for log lines.
const LOG_FORMAT_WITH_TIMESTAMP = /^[\d:.]+ .+/;
const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";

// Test the Copy menu item of the webconsole copies the expected clipboard text for
// different log messages.

add_task(async function() {
  await pushPref(PREF_MESSAGE_TIMESTAMP, true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Call the log function defined in the test page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.logStuff();
  });

  info("Test copy menu item with timestamp");
  await testMessagesCopy(hud, true);

  // Disable timestamp and wait until timestamp are not displayed anymore.
  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-timestamps"
  );
  await waitFor(
    () => hud.ui.outputNode.querySelector(".message .timestamp") === null
  );

  info("Test copy menu item without timestamp");
  await testMessagesCopy(hud, false);
});

async function testMessagesCopy(hud, timestamp) {
  const newLineString = "\n";

  info("Test copy menu item for the simple log");
  let message = await waitFor(() => findMessage(hud, "simple text message"));
  let clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");

  info("Check copied text for simple log message");
  let lines = clipboardText.split(newLineString);
  is(lines.length, 2, "There are 2 lines in the copied text");
  is(lines[1], "", "The last line is an empty new line");
  is(
    lines[0],
    `${
      timestamp ? getTimestampText(message) + " " : ""
    }simple text message test.js:3:15`,
    "Line of simple log message has expected text"
  );
  if (timestamp) {
    ok(
      LOG_FORMAT_WITH_TIMESTAMP.test(lines[0]),
      "Log line has the right format:\n" + lines[0]
    );
  }

  info("Test copy menu item for the console.trace message");
  message = await waitFor(() => findMessage(hud, "console.trace"));
  // Wait for the stacktrace to be rendered.
  await waitFor(() => message.querySelector(".frames"));
  clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");

  info("Check copied text for the console.trace message");
  lines = clipboardText.split(newLineString);
  is(lines.length, 4, "There are 4 lines in the copied text");
  is(lines[lines.length - 1], "", "The last line is an empty new line");
  is(
    lines[0],
    `${
      timestamp ? getTimestampText(message) + " " : ""
    }console.trace() test.js:6:17`,
    "Stacktrace first line has the expected text"
  );
  if (timestamp) {
    ok(
      LOG_FORMAT_WITH_TIMESTAMP.test(lines[0]),
      "Log line has the right format:\n" + lines[0]
    );
  }
  is(
    lines[1],
    `    wrapper ${TEST_URI}test.js:6`,
    "Stacktrace first line has the expected text"
  );
  is(
    lines[2],
    `    logStuff ${TEST_URI}test.js:9`,
    "Stacktrace second line has the expected text"
  );

  info("Test copy menu item for the error message");
  message = await waitFor(() => findMessage(hud, "Error:"));
  // Wait for the stacktrace to be rendered.
  await waitFor(() => message.querySelector(".frames"));
  clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");
  lines = clipboardText.split(newLineString);
  is(
    lines[0],
    `${timestamp ? getTimestampText(message) + " " : ""}Error: error object`,
    "Error object first line has expected text"
  );
  if (timestamp) {
    ok(
      LOG_FORMAT_WITH_TIMESTAMP.test(lines[0]),
      "Log line has the right format:\n" + lines[0]
    );
  }
  is(
    lines[1],
    `    wrapper ${TEST_URI}test.js:5`,
    "Error Stacktrace first line has the expected text"
  );
  is(
    lines[2],
    `    logStuff ${TEST_URI}test.js:9`,
    "Error Stacktrace second line has the expected text"
  );

  info("Test copy menu item for the reference error message");
  message = await waitFor(() => findMessage(hud, "ReferenceError:"));
  clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");
  lines = clipboardText.split(newLineString);
  is(
    lines[0],
    (timestamp ? getTimestampText(message) + " " : "") +
      "Uncaught ReferenceError: z is not defined",
    "ReferenceError first line has expected text"
  );
  if (timestamp) {
    ok(
      LOG_FORMAT_WITH_TIMESTAMP.test(lines[0]),
      "Log line has the right format:\n" + lines[0]
    );
  }
  is(
    lines[1],
    `    <anonymous> ${TEST_URI}test.js:11`,
    "ReferenceError second line has expected text"
  );
  ok(
    !!message.querySelector(".learn-more-link"),
    "There is a Learn More link in the ReferenceError message"
  );
  is(
    clipboardText.toLowerCase().includes("Learn More"),
    false,
    "The Learn More text wasn't put in the clipboard"
  );

  message = await waitFor(() => findMessage(hud, "repeated 2"));
  clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");
}

function getTimestampText(messageEl) {
  return getSelectionTextFromElement(messageEl.querySelector(".timestamp"));
}

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

/**
 * Return the string representation, as if it was selected with the mouse and copied,
 * using the Selection API.
 *
 * @param {HTMLElement} el
 * @returns {String} the text representation of the element.
 */
function getSelectionTextFromElement(el) {
  const doc = el.ownerDocument;
  const win = doc.defaultView;
  const range = doc.createRange();
  range.selectNode(el);
  const selection = win.getSelection();
  selection.addRange(range);
  const selectionText = selection.toString();
  selection.removeRange(range);
  return selectionText;
}
