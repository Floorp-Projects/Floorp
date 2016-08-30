/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* globals goDoCommand */

"use strict";

// Test copying of the entire console message when right-clicked
// with no other text selected. See Bug 1100562.

add_task(function* () {
  let hud;
  let outputNode;
  let contextMenu;

  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/test/test-console.html";

  const { tab, browser } = yield loadTab(TEST_URI);
  hud = yield openConsole(tab);
  outputNode = hud.outputNode;
  contextMenu = hud.iframeWindow.document.getElementById("output-contextmenu");

  registerCleanupFunction(() => {
    hud = outputNode = contextMenu = null;
  });

  hud.jsterm.clearOutput();

  yield ContentTask.spawn(browser, {}, function* () {
    let button = content.document.getElementById("testTrace");
    button.click();
  });

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: "bug 1100562",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        lines: 1,
      },
      {
        name: "console.trace output",
        consoleTrace: true,
        lines: 3,
      },
    ]
  });

  outputNode.focus();

  for (let result of results) {
    let message = [...result.matched][0];

    yield waitForContextMenu(contextMenu, message, () => {
      let copyItem = contextMenu.querySelector("#cMenu_copy");
      copyItem.doCommand();

      let controller = top.document.commandDispatcher
                                   .getControllerForCommand("cmd_copy");
      is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");
    });

    let clipboardText;

    yield waitForClipboardPromise(
      () => goDoCommand("cmd_copy"),
      (str) => {
        clipboardText = str;
        return message.textContent == clipboardText;
      }
    );

    ok(clipboardText, "Clipboard text was found and saved");

    let lines = clipboardText.split("\n");
    ok(lines.length > 0, "There is at least one newline in the message");
    is(lines.pop(), "", "There is a newline at the end");
    is(lines.length, result.lines, `There are ${result.lines} lines in the message`);

    // Test the first line for "timestamp message repeat file:line"
    let firstLine = lines.shift();
    ok(/^[\d:.]+ .+ \d+ .+:\d+$/.test(firstLine),
      "The message's first line has the right format");

    // Test the remaining lines (stack trace) for "TABfunctionName sourceURL:line:col"
    for (let line of lines) {
      ok(/^\t.+ .+:\d+:\d+$/.test(line), "The stack trace line has the right format");
    }
  }

  yield closeConsole(tab);
  yield finishTest();
});
