/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerPathHandler(`/`, function (request, response) {
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

httpServer.registerPathHandler("/test.js", function (request, response) {
  response.setHeader("Content-Type", "application/javascript");
  response.write(`
    window.logStuff = function() {
      function wrapper() {
        console.log("hello");
        console.log("myObject:", {a: 1}, "myArray:", ["b", "c"]);
        console.log(new Error("error object"));
        console.trace("myConsoleTrace");
        console.info("world", "!");
        /* add enough messages to trigger virtualization */
        for (let i = 0; i < 100; i++) {
          console.log("item-"+i);
        }
      }
      wrapper();
    };
  `);
});

const TEST_URI = `http://localhost:${httpServer.identity.primaryPort}/`;

const { MockFilePicker } = SpecialPowers;
MockFilePicker.init(window.browsingContext);
MockFilePicker.returnValue = MockFilePicker.returnOK;

var FileUtils = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
).FileUtils;

// Test the export visible messages to clipboard of the webconsole copies the expected
// clipboard text for different log messages to find if everything is copied to clipboard.

add_task(async function testExportToClipboard() {
  // Clear clipboard content.
  SpecialPowers.clipboardCopyString("");
  // Display timestamp to make sure we export them (there's a container query that would
  // hide them in the regular case, which we don't want).
  await pushPref("devtools.webconsole.timestampMessages", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  await clearOutput(hud);

  info("Call the log function defined in the test page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.logStuff();
  });

  info("Test export to clipboard ");
  // Let's wait until we have all the logged messages.
  const lastMessage = await waitFor(() =>
    findConsoleAPIMessage(hud, "item-99")
  );

  const clipboardText = await exportAllToClipboard(hud, lastMessage);
  ok(true, "Clipboard text was found and saved");

  checkExportedText(clipboardText);

  info("Test export to file");
  const fileText = await exportAllToFile(hud, lastMessage);
  checkExportedText(fileText);
});

function checkExportedText(text) {
  // Here we should have:
  //   -----------------------------------------------------
  //   hello test.js:4:17
  //   -----------------------------------------------------
  //   myObject:
  //   Object { a: 1 }
  //    myArray:
  //   Array [ "b", "c"]
  //   test.js:5:17
  //   -----------------------------------------------------
  //   Error: error object
  //       wrapper test.js:5
  //       logStuff test.js:14
  //   test.js:6:17
  //   -----------------------------------------------------
  //   console.trace() myConsoleTrace test.js:7:9
  //       wrapper test.js:7
  //       logStuff test.js:14
  //   -----------------------------------------------------
  //   world ! test.js:8:17
  //   -----------------------------------------------------
  //   item-0 test.js:11:19
  //   -----------------------------------------------------
  //   item-1 test.js:11:19
  //   -----------------------------------------------------
  //   […]
  //   -----------------------------------------------------
  //   item-99 test.js:11:19
  //   -----------------------------------------------------
  info("Check if all messages where exported as expected");
  let lines = text.split("\n").map(line => line.replace(/\r$/, ""));

  is(lines.length, 115, "There's 115 lines of text");
  is(lines.at(-1), "", "Last line is empty");

  info("Check that timestamp are displayed");
  const timestampRegex = /^\d{2}:\d{2}:\d{2}\.\d{3} /;
  // only check the first message
  ok(timestampRegex.test(lines[0]), "timestamp are included in the messages");
  lines = lines.map(l => l.replace(timestampRegex, ""));

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
  is(lines[8], `    logStuff ${TEST_URI}test.js:14`);
  is(lines[9], `test.js:6:17`);

  info("Check console.trace message");
  is(lines[10], `console.trace() myConsoleTrace test.js:7:17`);
  is(lines[11], `    wrapper ${TEST_URI}test.js:7`);
  is(lines[12], `    logStuff ${TEST_URI}test.js:14`);

  info("Check console.info message");
  is(lines[13], `world ! test.js:8:17`);

  const numberMessagesStartIndex = 14;
  for (let i = 0; i < 100; i++) {
    is(
      lines[numberMessagesStartIndex + i],
      `item-${i} test.js:11:19`,
      `Got expected text for line ${numberMessagesStartIndex + i}`
    );
  }
}

async function exportAllToFile(hud, message) {
  const menuPopup = await openContextMenu(hud, message);
  const exportFile = menuPopup.querySelector("#console-menu-export-file");
  ok(exportFile, "copy menu item is enabled");

  const nsiFile = new FileUtils.File(
    PathUtils.join(PathUtils.tempDir, `export_console_${Date.now()}.log`)
  );
  MockFilePicker.setFiles([nsiFile]);
  exportFile.click();
  info("Exporting to file");

  menuPopup.hidePopup();

  // The file may not be ready yet.
  await waitFor(() => IOUtils.exists(nsiFile.path));
  const buffer = await IOUtils.read(nsiFile.path);
  return new TextDecoder().decode(buffer);
}

/**
 * Simple helper method to open the context menu on a given message, and click on the
 * export visible messages to clipboard.
 */
async function exportAllToClipboard(hud, message) {
  const menuPopup = await openContextMenu(hud, message);
  const exportClipboard = menuPopup.querySelector(
    "#console-menu-export-clipboard"
  );
  ok(exportClipboard, "copy menu item is enabled");

  const clipboardText = await waitForClipboardPromise(
    () => exportClipboard.click(),
    data => data.includes("hello")
  );

  menuPopup.hidePopup();
  return clipboardText;
}
