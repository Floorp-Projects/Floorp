/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Debugger Search uses the identifier under cursor
 * if nothing is selected or manually passed
 */

"use strict";

function test() {

  const TAB_URL = EXAMPLE_URL + "doc_function-search.html";

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let Source = 'code_function-search-01.js';
    let Debugger = aPanel.panelWin;
    let Editor = Debugger.DebuggerView.editor;
    let Filtering = Debugger.DebuggerView.Filtering;

    waitForSourceShown(aPanel, Source).then(() => {
      Editor.setCursor({ line: 7, ch: 0});
      Filtering._doSearch("@");
      is(Filtering._searchbox.value, "@test", "Searchbox value should be set to the identifier under cursor if no aText or selection provided");
      closeDebuggerAndFinish(aPanel);
    });
  });
};
