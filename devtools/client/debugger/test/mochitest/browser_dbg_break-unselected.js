/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test breaking in code and jumping to the debugger before
 * the debugger UI has been initialized.
 */

const TAB_URL = EXAMPLE_URL + "doc_inline-debugger-statement.html";

function test() {
  Task.spawn(function* () {
    const tab = yield getTab(TAB_URL);
    const target = TargetFactory.forTab(tab);
    const toolbox = yield gDevTools.showToolbox(target, "webconsole");

    is(toolbox.currentToolId, "webconsole", "Console is the current panel");

    toolbox.target.on("thread-paused", Task.async(function* () {
      // Wait for the toolbox to handle the event and switch tools
      yield waitForTick();

      is(toolbox.currentToolId, "jsdebugger", "Debugger is the current panel");

      // Wait until it's actually fully loaded
      yield toolbox.loadTool("jsdebugger");

      const panel = toolbox.getCurrentPanel();
      const queries = panel.panelWin.require("./content/queries");
      const getState = panel.panelWin.DebuggerController.getState;

      is(panel.panelWin.gThreadClient.state, "paused",
         "Thread is still paused");

      yield waitForSourceAndCaret(panel, "debugger-statement.html", 16);
      is(queries.getSelectedSource(getState()).url, TAB_URL,
         "Selected source is the current tab url");
      is(queries.getSelectedSourceOpts(getState()).line, 16,
         "Line 16 is highlighted in the editor");

      resumeDebuggerThenCloseAndFinish(panel);
    }));

    callInTab(tab, "runDebuggerStatement");
  });
}
