/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerPathHandler(`/`, function(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`
    <meta charset=utf8>
    <h1>Test "copy message" context menu entry on message with framework stacktrace</h1>
    <script type="text/javascript" src="react.js"></script>
    <script type="text/javascript" src="test.js"></script>`);
});

httpServer.registerPathHandler("/test.js", function(_, response) {
  response.setHeader("Content-Type", "application/javascript");
  response.write(`
    window.myFunc = () => wrapper();
    const wrapper = () => console.trace("wrapperTrace");
  `);
});

httpServer.registerPathHandler("/react.js", function(_, response) {
  response.setHeader("Content-Type", "application/javascript");
  response.write(`
    window.render = function() {
      const renderFinal = () => window.myFunc();
      renderFinal();
    };
  `);
});

const TEST_URI = `http://localhost:${httpServer.identity.primaryPort}/`;

// Test the Copy menu item of the webconsole copies the expected clipboard text for
// a message with a "framework" stacktrace (i.e. with grouped frames).

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Call the log function defined in the test page");
  const onMessage = waitForMessage(hud, "wrapperTrace");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.render();
  });
  const message = await onMessage;
  const messageEl = message.node;
  await waitFor(() => messageEl.querySelector(".frames"));

  let clipboardText = await copyMessageContent(hud, messageEl);
  ok(true, "Clipboard text was found and saved");

  const newLineString = "\n";
  info("Check copied text for the console.trace message");
  let lines = clipboardText.split(newLineString);
  is(lines.length, 5, "Correct number of lines in the copied text");
  is(lines[lines.length - 1], "", "The last line is an empty new line");
  is(
    lines[0],
    `console.trace() wrapperTrace test.js:3:35`,
    "Message first line has the expected text"
  );
  is(
    lines[1],
    `    wrapper ${TEST_URI}test.js:3`,
    "Stacktrace first line has the expected text"
  );
  is(
    lines[2],
    `    myFunc ${TEST_URI}test.js:2`,
    "Stacktrace second line has the expected text"
  );
  is(lines[3], `    React 2`, "Stacktrace third line has the expected text");

  info("Expand the React group");
  const getFrames = () => messageEl.querySelectorAll(".frame");
  const frames = getFrames().length;
  messageEl.querySelector(".frames .group").click();
  // Let's wait until all React frames are displayed.
  await waitFor(() => getFrames().length > frames);

  clipboardText = await copyMessageContent(hud, messageEl);
  ok(true, "Clipboard text was found and saved");

  info(
    "Check copied text for the console.trace message with expanded React frames"
  );
  lines = clipboardText.split(newLineString);
  is(lines.length, 7, "Correct number of lines in the copied text");
  is(lines[lines.length - 1], "", "The last line is an empty new line");
  is(
    lines[0],
    `console.trace() wrapperTrace test.js:3:35`,
    "Message first line has the expected text"
  );
  is(
    lines[1],
    `    wrapper ${TEST_URI}test.js:3`,
    "Stacktrace first line has the expected text"
  );
  is(
    lines[2],
    `    myFunc ${TEST_URI}test.js:2`,
    "Stacktrace second line has the expected text"
  );
  is(lines[3], `    React 2`, "Stacktrace third line has the expected text");
  is(
    lines[4],
    `        renderFinal`,
    "Stacktrace fourth line has the expected text"
  );
  is(lines[5], `        render`, "Stacktrace fifth line has the expected text");
});

/**
 * Simple helper method to open the context menu on a given message, and click on the copy
 * menu item.
 */
async function copyMessageContent(hud, messageEl) {
  const menuPopup = await openContextMenu(hud, messageEl);
  const copyMenuItem = menuPopup.querySelector("#console-menu-copy");
  ok(copyMenuItem, "copy menu item is enabled");

  return waitForClipboardPromise(() => copyMenuItem.click(), data => data);
}
