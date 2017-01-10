/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<script>
  window.logStuff = function () {
    console.log("simple text message");
    console.trace();
  };
</script>`;

// Test the Copy menu item of the webconsole copies the expected clipboard text for
// different log messages.

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);
  hud.jsterm.clearOutput();

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.logStuff();
  });

  let messages = [];
  for (let s of ["simple text message", "console.trace()"]) {
    messages.push(yield waitFor(() => findMessage(hud, s)));
  }

  for (let message of messages) {
    let menuPopup = yield openContextMenu(hud, message);
    let copyMenuItem = menuPopup.querySelector("#console-menu-copy");
    ok(copyMenuItem, "copy menu item is enabled");

    yield waitForClipboardPromise(
      () => copyMenuItem.click(),
      data => data === message.textContent
    );

    ok(true, "Clipboard text was found and saved");

    // TODO: The copy menu item & this test should be improved for the new console.
    // This is tracked by https://bugzilla.mozilla.org/show_bug.cgi?id=1329606 .

    // The rest of this test was copied from the old console frontend. The copy menu item
    // logic is not on par with the one from the old console yet.

    // let lines = clipboardText.split("\n");
    // ok(lines.length > 0, "There is at least one newline in the message");
    // is(lines.pop(), "", "There is a newline at the end");
    // is(lines.length, result.lines, `There are ${result.lines} lines in the message`);

    // // Test the first line for "timestamp message repeat file:line"
    // let firstLine = lines.shift();
    // ok(/^[\d:.]+ .+ \d+ .+:\d+$/.test(firstLine),
    //   "The message's first line has the right format:\n" + firstLine);

    // // Test the remaining lines (stack trace) for "TABfunctionName sourceURL:line:col"
    // for (let line of lines) {
    //   ok(/^\t.+ .+:\d+:\d+$/.test(line),
    //     "The stack trace line has the right format:\n" + line);
    // }
  }
});
