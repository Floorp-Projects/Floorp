/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test adding breakpoints from the source editor context menu
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gContextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");
    const queries = gDebugger.require("./content/queries");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function* () {
      yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1);

      is(gDebugger.gThreadClient.state, "paused",
         "Should only be getting stack frames while paused.");
      is(queries.getSourceCount(getState()), 2,
         "Found the expected number of sources.");
      isnot(gEditor.getText().indexOf("debugger"), -1,
            "The correct source was loaded initially.");
      is(gSources.selectedValue, gSources.values[1],
         "The correct source is selected.");

      ok(gContextMenu,
         "The source editor's context menupopup is available.");

      gEditor.focus();
      gEditor.setSelection({ line: 1, ch: 0 }, { line: 1, ch: 10 });

      gContextMenu.openPopup(gEditor.container, "overlap", 0, 0, true, false);
      gEditor.emit("gutterClick", 6, 2);

      yield once(gContextMenu, "popupshown");
      is(queries.getBreakpoints(getState()).length, 0, "no breakpoints added");

      let cmd = gContextMenu.querySelector("menuitem[command=addBreakpointCommand]");
      EventUtils.synthesizeMouseAtCenter(cmd, {}, gDebugger);
      yield waitForDispatch(gPanel, gDebugger.constants.ADD_BREAKPOINT);

      is(queries.getBreakpoints(getState()).length, 1,
         "1 breakpoint correctly added");
      ok(queries.getBreakpoint(getState(),
                               { actor: gSources.values[1], line: 7 }),
         "Breakpoint on line 7 exists");

      gContextMenu.openPopup(gEditor.container, "overlap", 0, 0, true, false);
      gEditor.emit("gutterClick", 7, 2);

      yield once(gContextMenu, "popupshown");
      is(queries.getBreakpoints(getState()).length, 1,
         "1 breakpoint correctly added");

      cmd = gContextMenu.querySelector("menuitem[command=addConditionalBreakpointCommand]");
      EventUtils.synthesizeMouseAtCenter(cmd, {}, gDebugger);
      yield waitForDispatch(gPanel, gDebugger.constants.ADD_BREAKPOINT);

      is(queries.getBreakpoints(getState()).length, 2,
         "2 breakpoints correctly added");
      ok(queries.getBreakpoint(getState(),
                               { actor: gSources.values[1], line: 8 }),
         "Breakpoint on line 8 exists");

      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    callInTab(gTab, "firstCall");
  });
}
