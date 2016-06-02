/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Debugger Search uses the identifier under cursor if nothing is
 * selected or manually passed and searching using certain operators.
 */
"use strict";

function test() {
  const TAB_URL = EXAMPLE_URL + "doc_function-search.html";

  let options = {
    source: EXAMPLE_URL + "code_function-search-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    let Debugger = aPanel.panelWin;
    let Editor = Debugger.DebuggerView.editor;
    let Filtering = Debugger.DebuggerView.Filtering;

    function doSearch(aOperator) {
      Editor.dropSelection();
      Filtering._doSearch(aOperator);
    }

    info("Testing with cursor at the beginning of the file...");

    doSearch();
    is(Filtering._searchbox.value, "",
      "The searchbox value should not be auto-filled when searching for files.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch("!");
    is(Filtering._searchbox.value, "!",
      "The searchbox value should not be auto-filled when searching across all files.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch("@");
    is(Filtering._searchbox.value, "@",
      "The searchbox value should not be auto-filled when searching for functions.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch("#");
    is(Filtering._searchbox.value, "#",
      "The searchbox value should not be auto-filled when searching inside a file.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch(":");
    is(Filtering._searchbox.value, ":",
      "The searchbox value should not be auto-filled when searching for a line.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch("*");
    is(Filtering._searchbox.value, "*",
      "The searchbox value should not be auto-filled when searching for variables.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    Editor.setCursor({ line: 7, ch: 0});
    info("Testing with cursor at line 8 and char 1...");

    doSearch();
    is(Filtering._searchbox.value, "",
      "The searchbox value should not be auto-filled when searching for files.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch("!");
    is(Filtering._searchbox.value, "!test",
      "The searchbox value was incorrect when searching across all files.");
    is(Filtering._searchbox.selectionStart, 1,
      "The searchbox operator should not be selected");
    is(Filtering._searchbox.selectionEnd, 5,
      "The searchbox contents should be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch("@");
    is(Filtering._searchbox.value, "@test",
      "The searchbox value was incorrect when searching for functions.");
    is(Filtering._searchbox.selectionStart, 1,
      "The searchbox operator should not be selected");
    is(Filtering._searchbox.selectionEnd, 5,
      "The searchbox contents should be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch("#");
    is(Filtering._searchbox.value, "#test",
      "The searchbox value should be auto-filled when searching inside a file.");
    is(Filtering._searchbox.selectionStart, 1,
      "The searchbox operator should not be selected");
    is(Filtering._searchbox.selectionEnd, 5,
      "The searchbox contents should be selected");
    is(Editor.getSelection(), "test",
      "The selection in the editor should be 'test'.");

    doSearch(":");
    is(Filtering._searchbox.value, ":",
      "The searchbox value should not be auto-filled when searching for a line.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    doSearch("*");
    is(Filtering._searchbox.value, "*",
      "The searchbox value should not be auto-filled when searching for variables.");
    is(Filtering._searchbox.selectionStart, Filtering._searchbox.selectionEnd,
      "The searchbox contents should not be selected");
    is(Editor.getSelection(), "",
      "The selection in the editor should be empty.");

    closeDebuggerAndFinish(aPanel);
  });
}
