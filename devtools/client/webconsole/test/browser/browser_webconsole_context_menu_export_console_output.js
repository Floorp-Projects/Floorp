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

const { MockFilePicker } = SpecialPowers;
MockFilePicker.init(window);
MockFilePicker.returnValue = MockFilePicker.returnOK;

var { Cu } = require("chrome");
var FileUtils = Cu.import("resource://gre/modules/FileUtils.jsm").FileUtils;

// Test the export visible messages to clipboard of the webconsole copies the expected
// clipboard text for different log messages to find if everything is copied to clipboard.

add_task(async function testExportToClipboard() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await clearOutput(hud);

  info("Call the log function defined in the test page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.logStuff();
  });

  info("Test export to clipboard ");
  // Let's wait until we have all the logged messages.
  await waitFor(() => findMessages(hud, "").length === 5);
  // And also until the stacktraces are rendered (there should be 2)
  await waitFor(
    () => hud.ui.outputNode.querySelectorAll(".frames").length === 2
  );

  const message = findMessage(hud, "hello");
  const clipboardText = await exportAllToClipboard(hud, message);
  ok(true, "Clipboard text was found and saved");

  checkExportedText(clipboardText);
});

add_task(async function testExportToFile() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await clearOutput(hud);

  info("Call the log function defined in the test page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.logStuff();
  });

  info("Test export to clipboard ");
  // Let's wait until we have all the logged messages.
  await waitFor(() => findMessages(hud, "").length === 5);
  // And also until the stacktraces are rendered (there should be 2)
  await waitFor(
    () => hud.ui.outputNode.querySelectorAll(".frames").length === 2
  );

  const message = findMessage(hud, "hello");
  const text = await exportAllToFile(hud, message);
  checkExportedText(text);
});

function checkExportedText(text) {
  // Here we should have:
  //   -------------------------------------------------------------------
  //   hello test.js:4:17
  //   -------------------------------------------------------------------
  //   myObject:
  //   Object { a: 1 }
  //    myArray:
  //   Array [ "b", "c"]
  //   test.js:5:17
  //   -------------------------------------------------------------------
  //   Error: error object
  //       wrapper test.js:5
  //       logStuff test.js:17
  //   test.js:6:17
  //   -------------------------------------------------------------------
  //   console.trace() myConsoleTrace test.js:7:9
  //       wrapper test.js:7
  //       logStuff test.js:17
  //   -------------------------------------------------------------------
  //   world ! test.js:8:17
  //   -------------------------------------------------------------------
  info("Check if all messages where exported as expected");
  const lines = text.split("\n").map(line => line.replace(/\r$/, ""));

  is(lines.length, 15, "There's 15 lines of text");
  is(lines[lines.length - 1], "", "Last line is empty");

  info("Check simple text message");
  is(lines[0], "hello test.js:4:17", "Simple log has expected text");

  info("Check multiple logged items message");
  is(lines[1], `myObject: `);
  is(lines[2], `Object { a: 1 }`);
  is(lines[3], ` myArray: `);
  is(lines[4], `Array [ "b", "c" ]`);
  is(lines[5], `test.js:5:17`);

  info("Check logged error object");
  is(lines[6], `Error: error object`);
  is(lines[7], `    wrapper ${TEST_URI}test.js:6`);
  is(lines[8], `    logStuff ${TEST_URI}test.js:10`);
  is(lines[9], `test.js:6:17`);

  info("Check console.trace message");
  is(lines[10], `console.trace() myConsoleTrace test.js:7:17`);
  is(lines[11], `    wrapper ${TEST_URI}test.js:7`);
  is(lines[12], `    logStuff ${TEST_URI}test.js:10`);

  info("Check console.info message");
  is(lines[13], `world ! test.js:8:17`);
}

async function exportAllToFile(hud, message) {
  const menuPopup = await openContextMenuExportSubMenu(hud, message);
  const exportFile = menuPopup.querySelector("#console-menu-export-file");
  ok(exportFile, "copy menu item is enabled");

  const nsiFile = FileUtils.getFile("TmpD", [
    `export_console_${Date.now()}.log`,
  ]);
  MockFilePicker.setFiles([nsiFile]);
  exportFile.click();
  info("Exporting to file");

  // The file may not be ready yet.
  await waitFor(() => OS.File.exists(nsiFile.path));
  const buffer = await OS.File.read(nsiFile.path);
  return new TextDecoder().decode(buffer);
}

/**
 * Simple helper method to open the context menu on a given message, and click on the
 * export visible messages to clipboard.
 */
async function exportAllToClipboard(hud, message) {
  const menuPopup = await openContextMenuExportSubMenu(hud, message);
  const exportClipboard = menuPopup.querySelector(
    "#console-menu-export-clipboard"
  );
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

async function openContextMenuExportSubMenu(hud, message) {
  const menuPopup = await openContextMenu(hud, message);
  const exportMenu = menuPopup.querySelector("#console-menu-export");

  const view = exportMenu.ownerDocument.defaultView;
  EventUtils.synthesizeMouseAtCenter(exportMenu, { type: "mousemove" }, view);
  return menuPopup;
}
