/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the DebuggerView error loading source text is correct.
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
    const gView = gDebugger.DebuggerView;
    const gEditor = gDebugger.DebuggerView.editor;
    const gL10N = gDebugger.L10N;
    const require = gDebugger.require;
    const actions = bindActionCreators(gPanel);
    const constants = require("./content/constants");
    const controller = gDebugger.DebuggerController;

    function showBogusSource() {
      const source = { actor: "fake.actor", url: "http://fake.url/" };
      actions.newSource(source);

      controller.dispatch({
        type: constants.LOAD_SOURCE_TEXT,
        source: source,
        status: "start"
      });

      controller.dispatch({
        type: constants.SELECT_SOURCE,
        source: source
      });

      controller.dispatch({
        type: constants.LOAD_SOURCE_TEXT,
        source: source,
        status: "error",
        error: "bogus actor"
      });
    }

    function testDebuggerLoadingError() {
      ok(gEditor.getText().includes(gL10N.getFormatStr("errorLoadingText2", "")),
         "The valid error loading message is displayed.");
    }

    Task.spawn(function* () {
      showBogusSource();
      testDebuggerLoadingError();
      closeDebuggerAndFinish(gPanel);
    });
  });
}
