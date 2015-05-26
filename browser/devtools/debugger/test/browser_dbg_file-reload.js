/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that source contents are invalidated when the target navigates.
 */

const TAB_URL = EXAMPLE_URL + "doc_random-javascript.html";
const JS_URL = EXAMPLE_URL + "sjs_random-javascript.sjs";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gEditor = gDebugger.DebuggerView.editor;
    let gSources = gDebugger.DebuggerView.Sources;
    let gControllerSources = gDebugger.DebuggerController.SourceScripts;

    Task.spawn(function() {
      yield waitForSourceShown(aPanel, JS_URL);

      is(gSources.itemCount, 1,
        "There should be one source displayed in the view.")
      is(getSelectedSourceURL(gSources), JS_URL,
        "The correct source is currently selected in the view.");
      ok(gEditor.getText().includes("bacon"),
        "The currently shown source contains bacon. Mmm, delicious!");

      let { source } = gSources.selectedItem.attachment;
      let [, firstText] = yield gControllerSources.getText(source);
      let firstNumber = parseFloat(firstText.match(/\d\.\d+/)[0]);

      is(firstText, gEditor.getText(),
        "gControllerSources.getText() returned the expected contents.");
      ok(firstNumber <= 1 && firstNumber >= 0,
        "The generated number seems to be created correctly.");

      yield reloadActiveTab(aPanel, gDebugger.EVENTS.SOURCE_SHOWN);

      is(gSources.itemCount, 1,
        "There should be one source displayed in the view after reloading.")
      is(getSelectedSourceURL(gSources), JS_URL,
        "The correct source is currently selected in the view after reloading.");
      ok(gEditor.getText().includes("bacon"),
        "The newly shown source contains bacon. Mmm, delicious!");

      ({ source } = gSources.selectedItem.attachment);
      let [, secondText] = yield gControllerSources.getText(source);
      let secondNumber = parseFloat(secondText.match(/\d\.\d+/)[0]);

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
