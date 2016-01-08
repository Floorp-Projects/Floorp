/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the DebuggerView error loading source text is correct.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gView = gDebugger.DebuggerView;
    const gEditor = gDebugger.DebuggerView.editor;
    const gL10N = gDebugger.L10N;
    const actions = bindActionCreators(gPanel);

    function showBogusSource() {
      let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_ERROR_SHOWN);
      actions.newSource({ url: "http://example.com/fake.js", actor: "fake.actor" });
      actions.selectSource({ actor: "fake.actor" });
      return finished;
    }

    function testDebuggerLoadingError() {
      ok(gEditor.getText().includes(gL10N.getFormatStr("errorLoadingText2", "noSuchActor")),
         "The valid error loading message is displayed.");
    }

    Task.spawn(function*() {
      yield waitForSourceShown(gPanel, "-01.js");
      yield showBogusSource();
      yield testDebuggerLoadingError();
      closeDebuggerAndFinish(gPanel);
    });
  });
}
