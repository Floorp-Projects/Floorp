/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
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

  content.location = "data:text/html;charset=utf8,test Scratchpad pretty print.";
}

function runTests(sw)
{
  const sp = sw.Scratchpad;
  sp.setText([
    "// line 1",
    "// line 2",
    "var re = /a bad /regexp/; // line 3 is an obvious syntax error!",
    "// line 4",
    "// line 5",
    ""
  ].join("\n"));
  sp.run().then(() => {
    // CodeMirror lines and columns are 0-based, Scratchpad UI and error
    // stack are 1-based.
    let errorLine = 3;
    let editorDoc = sp.editor.container.contentDocument;
    sp.editor.jumpToLine();
    let lineInput = editorDoc.querySelector("input");
    let inputLine = lineInput.value;
    is(inputLine, errorLine, "jumpToLine input field is set from editor selection");
    EventUtils.synthesizeKey("VK_RETURN", { }, editorDoc.defaultView);
    // CodeMirror lines and columns are 0-based, Scratchpad UI and error
    // stack are 1-based.
    let cursor = sp.editor.getCursor();
    is(cursor.line + 1, inputLine, "jumpToLine goto error location (line)");
    is(cursor.ch + 1, 1, "jumpToLine goto error location (column)");
    finish();
  });
}
