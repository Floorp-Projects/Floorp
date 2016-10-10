/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that text entered in the debugger's searchbox is properly parsed.
 */

function test() {
  initDebugger().then(([aTab,, aPanel]) => {
    let filterView = aPanel.panelWin.DebuggerView.Filtering;
    let searchbox = aPanel.panelWin.DebuggerView.Filtering._searchbox;

    setText(searchbox, "");
    is(filterView.searchData.toSource(), '["", [""]]',
      "The searchbox data wasn't parsed correctly (1).");

    setText(searchbox, "#token");
    is(filterView.searchData.toSource(), '["#", ["", "token"]]',
      "The searchbox data wasn't parsed correctly (2).");

    setText(searchbox, ":42");
    is(filterView.searchData.toSour
      "The searchbox data wasn't parsed correctly (3).");

    setText(searchbox, "#token:42");
    is(filterView.searchData.toSource(), '["#", ["", "token:42"]]',
      "The searchbox data wasn't parsed correctly (4).");

    setText(searchbox, ":42#token");
    is(filterView.searchData.toSource(), '["#", [":42", "token"]]',
      "The searchbox data wasn't parsed correctly (5).");

    setText(searchbox, "#token:42#token:42");
    is(filterView.searchData.toSource(), '["#", ["#token:42", "token:42"]]',
      "The searchbox data wasn't parsed correctly (6).");

    setText(searchbox, ":42#token:42#token");
    is(filterView.searchData.toSource(), '["#", [":42#token:42", "token"]]',
      "The searchbox data wasn't parsed correctly (7).");


    setText(searchbox, "file");
    is(filterView.searchData.toSource(), '["", ["file"]]',
      "The searchbox data wasn't parsed correctly (8).");

    setText(searchbox, "file#token");
    is(filterView.searchData.toSource(), '["#", ["file", "token"]]',
      "The searchbox data wasn't parsed correctly (9).");

    setText(searchbox, "file:42");
    is(filterView.searchData.toSource(), '[":", ["file", 42]]',
      "The searchbox data wasn't parsed correctly (10).");

    setText(searchbox, "file#token:42");
    is(filterView.searchData.toSource(), '["#", ["file", "token:42"]]',
      "The searchbox data wasn't parsed correctly (11).");

    setText(searchbox, "file:42#token");
    is(filterView.searchData.toSource(), '["#", ["file:42", "token"]]',
      "The searchbox data wasn't parsed correctly (12).");

    setText(searchbox, "file#token:42#token:42");
    is(filterView.searchData.toSource(), '["#", ["file#token:42", "token:42"]]',
      "The searchbox data wasn't parsed correctly (13).");

    setText(searchbox, "file:42#token:42#token");
    is(filterView.searchData.toSource(), '["#", ["file:42#token:42", "token"]]',
      "The searchbox data wasn't parsed correctly (14).");


    setText(searchbox, "!token");
    is(filterView.searchData.toSource(), '["!", ["token"]]',
      "The searchbox data wasn't parsed correctly (15).");

    setText(searchbox, "!token#global");
    is(filterView.searchData.toSource(), '["!", ["token#global"]]',
      "The searchbox data wasn't parsed correctly (16).");

    setText(searchbox, "!token#global:42");
    is(filterView.searchData.toSource(), '["!", ["token#global:42"]]',
      "The searchbox data wasn't parsed correctly (17).");

    setText(searchbox, "!token:42#global");
    is(filterView.searchData.toSource(), '["!", ["token:42#global"]]',
      "The searchbox data wasn't parsed correctly (18).");


    setText(searchbox, "@token");
    is(filterView.searchData.toSource(), '["@", ["token"]]',
      "The searchbox data wasn't parsed correctly (19).");

    setText(searchbox, "@token#global");
    is(filterView.searchData.toSource(), '["@", ["token#global"]]',
      "The searchbox data wasn't parsed correctly (20).");

    setText(searchbox, "@token#global:42");
    is(filterView.searchData.toSource(), '["@", ["token#global:42"]]',
      "The searchbox data wasn't parsed correctly (21).");

    setText(searchbox, "@token:42#global");
    is(filterView.searchData.toSource(), '["@", ["token:42#global"]]',
      "The searchbox data wasn't parsed correctly (22).");


    setText(searchbox, "*token");
    is(filterView.searchData.toSource(), '["*", ["token"]]',
      "The searchbox data wasn't parsed correctly (23).");

    setText(searchbox, "*token#global");
    is(filterView.searchData.toSource(), '["*", ["token#global"]]',
      "The searchbox data wasn't parsed correctly (24).");

    setText(searchbox, "*token#global:42");
    is(filterView.searchData.toSource(), '["*", ["token#global:42"]]',
      "The searchbox data wasn't parsed correctly (25).");

    setText(searchbox, "*token:42#global");
    is(filterView.searchData.toSource(), '["*", ["token:42#global"]]',
      "The searchbox data wasn't parsed correctly (26).");


    closeDebuggerAndFinish(aPanel);
  });
}
