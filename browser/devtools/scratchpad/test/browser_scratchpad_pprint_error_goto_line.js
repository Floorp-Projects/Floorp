/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html;charset=utf8,"
    + "test Scratchpad pretty print error goto line.";
}

function testJumpToPrettyPrintError(sp, error, remark) {
  info("will test jumpToLine after prettyPrint error" + remark);

  // CodeMirror lines and columns are 0-based, Scratchpad UI and error
  // stack are 1-based.
  is(/Invalid regular expression flag \(3:10\)/.test(error), true,
     "prettyPrint expects error in editor text:\n" + error);

  sp.editor.jumpToLine();

  const editorDoc = sp.editor.container.contentDocument;
  const lineInput = editorDoc.querySelector("input");
  const errorLocation = lineInput.value;
  const [ inputLine, inputColumn ] = errorLocation.split(":");
  const errorLine = 3, errorColumn = 10;

  is(inputLine, errorLine,
     "jumpToLine input field is set from editor selection (line)");
  is(inputColumn, errorColumn,
     "jumpToLine input field is set from editor selection (column)");

  EventUtils.synthesizeKey("VK_RETURN", { }, editorDoc.defaultView);

  // CodeMirror lines and columns are 0-based, Scratchpad UI and error
  // stack are 1-based.
  const cursor = sp.editor.getCursor();
  is(inputLine, cursor.line + 1, "jumpToLine goto error location (line)");
  is(inputColumn, cursor.ch + 1, "jumpToLine goto error location (column)");
}

function runTests(sw, sp)
{
  sp.setText([
    "// line 1",
    "// line 2",
    "var re = /a bad /regexp/; // line 3 is an obvious syntax error!",
    "// line 4",
    "// line 5",
    ""
  ].join("\n"));

  sp.prettyPrint().then(aFulfill => {
    ok(false, "Expecting Invalid regexp flag (3:10)");
    finish();
  }, error => {
    testJumpToPrettyPrintError(sp, error, " (Bug 1005471, first time)");
  });

  sp.prettyPrint().then(aFulfill => {
    ok(false, "Expecting Invalid regexp flag (3:10)");
    finish();
  }, error => {
    // Second time verifies bug in earlier implementation fixed.
    testJumpToPrettyPrintError(sp, error, " (second time)");
    finish();
  });
}
