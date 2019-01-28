/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerPathHandler(`/`, function(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`
    <html>
      <head>
        <meta charset="utf-8">
        <script type="text/javascript" src="test.js"></script>
      </head>
      <body>Test "Export All" context menu entry</body>
    </html>`);
});

httpServer.registerPathHandler("/test.js", function(request, response) {
  response.setHeader("Content-Type", "application/javascript");
  response.write(`
    window.logStuff = function() {
      function wrapper() {
        console.log("hello");
        console.log("myObject:", {a: 1}, "myArray:", ["b", "c"]);
        console.log(new Error("error object"));
        console.trace("myConsoleTrace");
        console.info("world", "!");
      }
      wrapper();
    };
  `);
});

const TEST_URI = `http://localhost:${httpServer.identity.primaryPort}/`;

// Test the export visible messages to clipboard of the webconsole copies the expected
// clipboard text for different log messages to find if everything is copied to clipboard.

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  info("Call the log function defined in the test page");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.logStuff();
  });

  info("Test export to clipboard ");
  // Let's wait until we have all the logged messages.
  await waitFor(() => findMessages(hud, "").length === 5);
  // And also until the stacktraces are rendered (there should be 2)
  await waitFor(() => hud.ui.outputNode.querySelectorAll(".frames").length === 2);

  const message = findMessage(hud, "hello");
  const clipboardText = await exportAllToClipboard(hud, message);
  ok(true, "Clipboard text was found and saved");

// Here we should have:
//   -------------------------------------------------------------------
//   hello test.js:4:9
//   -------------------------------------------------------------------
//   myObject:
//   Object { a: 1 }
//    myArray:
//   Array [ "b", "c"]
//   test.js:5:9
//   -------------------------------------------------------------------
//   Error: "error object":
//       wrapper test.js:5
//       logStuff test.js:9
//   test.js:6:9
//   -------------------------------------------------------------------
//   console.trace() myConsoleTrace test.js:7:9
//       wrapper test.js:7
//       logStuff test.js:9
//   -------------------------------------------------------------------
//   world ! test.js:8:9
//   -------------------------------------------------------------------

  info("Check if all messages where copied to clipboard");
  const clipboardLines = clipboardText.split("\n");
  is(clipboardLines.length, 15, "There's 15 lines of text");
  is(clipboardLines[clipboardLines.length - 1], "", "Last line is empty");

  info("Check simple text message");
  is(clipboardLines[0], "hello test.js:4:9", "Simple log has expected text");

  info("Check multiple logged items message");
  is(clipboardLines[1], `myObject: `);
  is(clipboardLines[2], `Object { a: 1 }`);
  is(clipboardLines[3], ` myArray: `);
  is(clipboardLines[4], `Array [ "b", "c" ]`);
  is(clipboardLines[5], `test.js:5:9`);

  info("Check logged error object");
  is(clipboardLines[6], `Error: "error object"`);
  is(clipboardLines[7], `    wrapper ${TEST_URI}test.js:6`);
  is(clipboardLines[8], `    logStuff ${TEST_URI}test.js:10`);
  is(clipboardLines[9], `test.js:6:9`);

  info("Check console.trace message");
  is(clipboardLines[10], `console.trace() myConsoleTrace test.js:7:9`);
  is(clipboardLines[11], `    wrapper ${TEST_URI}test.js:7`);
  is(clipboardLines[12], `    logStuff ${TEST_URI}test.js:10`);

  info("Check console.info message");
  is(clipboardLines[13], `world ! test.js:8:9`);
});

/**
 * Simple helper method to open the context menu on a given message, and click on the
 * export visible messages to clipboard.
 */
async function exportAllToClipboard(hud, message) {
  const menuPopup = await openContextMenu(hud, message);
  const exportClipboard = menuPopup.querySelector("#console-menu-export-clipboard");
  ok(exportClipboard, "copy menu item is enabled");

  let clipboardText;
  await waitForClipboardPromise(
    () => exportClipboard.click(),
    data => {
      clipboardText = data;
      return data;
    }
  );
  return clipboardText;
}
