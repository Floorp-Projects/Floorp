/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Map Control + A to Select All, In the web console input

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Test console select all";

add_task(async function() {
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test.
  gBrowser.selectedTab.focus();
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  setInputValue(hud, "Ignore These Four Words");

  // Test select all with (cmd|control) + a.
  EventUtils.synthesizeKey("a", { accelKey: true });

  const inputLength = getSelectionTextLength(jsterm);
  is(inputLength, getInputValue(hud).length, "Select all of input");

  // (cmd|control) + e cannot be disabled on Linux so skip this section on that OS.
  if (Services.appinfo.OS !== "Linux") {
    // Test do nothing on Control + E.
    setInputValue(hud, "Ignore These Four Words");
    setCursorAtStart(jsterm);
    EventUtils.synthesizeKey("e", { accelKey: true });
    checkSelectionStart(
      jsterm,
      0,
      "control|cmd + e does not move to end of input"
    );
  }
});

function getSelectionTextLength(jsterm) {
  return jsterm.editor.getSelection().length;
}

function setCursorAtStart(jsterm) {
  jsterm.editor.setCursor({ line: 0, ch: 0 });
}

function checkSelectionStart(jsterm, expectedCursorIndex, assertionInfo) {
  const [selection] = jsterm.editor.codeMirror.listSelections();
  const { head } = selection;
  is(head.ch, expectedCursorIndex, assertionInfo);
}
