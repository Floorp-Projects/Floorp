/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that source contents are invalidated when the target navigates.
 */

const TAB_URL = EXAMPLE_URL + "doc_random-javascript.html";
const JS_URL = EXAMPLE_URL + "sjs_random-javascript.sjs";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gPanel = aPanel;
    const gDebugger = aPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require('./content/queries');
    const constants = gDebugger.require('./content/constants');
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function*() {
      yield waitForSourceShown(gPanel, JS_URL);
      let source = queries.getSelectedSource(getState());

      is(queries.getSourceCount(getState()), 1,
        "There should be one source displayed in the view.")
      is(source.url, JS_URL,
        "The correct source is currently selected in the view.");
      ok(gEditor.getText().includes("bacon"),
        "The currently shown source contains bacon. Mmm, delicious!");

      const { text: firstText } = yield queries.getSourceText(getState(), source.actor);
      const firstNumber = parseFloat(firstText.match(/\d\.\d+/)[0]);

      is(firstText, gEditor.getText(),
        "gControllerSources.getText() returned the expected contents.");
      ok(firstNumber <= 1 && firstNumber >= 0,
        "The generated number seems to be created correctly.");

      yield reloadActiveTab(aPanel, gDebugger.EVENTS.SOURCE_SHOWN);

      is(queries.getSourceCount(getState()), 1,
        "There should be one source displayed in the view.")
      is(source.url, JS_URL,
        "The correct source is currently selected in the view.");
      ok(gEditor.getText().includes("bacon"),
        "The newly shown source contains bacon. Mmm, delicious!");

      source = queries.getSelectedSource(getState());
      const { text: secondText } = yield queries.getSourceText(getState(), source.actor);
      const secondNumber = parseFloat(secondText.match(/\d\.\d+/)[0]);

      is(secondText, gEditor.getText(),
        "gControllerSources.getText() returned the expected contents.");
      ok(secondNumber <= 1 && secondNumber >= 0,
        "The generated number seems to be created correctly.");

      isnot(firstText, secondText,
        "The displayed sources were different across reloads.");
      isnot(firstNumber, secondNumber,
        "The displayed sources differences were correct across reloads.");

      yield closeDebuggerAndFinish(aPanel);
    });
  });
}
