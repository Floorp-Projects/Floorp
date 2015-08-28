/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure eval scripts with the sourceURL pragma are correctly
 * displayed
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints, gEditor;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gEditor = gDebugger.DebuggerView.editor;

    return Task.spawn(function*() {
      yield waitForSourceShown(gPanel, "-eval.js");
      is(gSources.values.length, 1, "Should have 1 source");

      let newSource = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.NEW_SOURCE);
      callInTab(gTab, "evalSourceWithSourceURL");
      yield newSource;

      is(gSources.values.length, 2, "Should have 2 sources");

      let item = gSources.getItemForAttachment(e => e.label == "bar.js");
      ok(item, "Source label is incorrect.");
      is(item.attachment.group, 'http://example.com',
         'Source group is incorrect');

      let shown = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
      gSources.selectedItem = item;
      yield shown;

      ok(gEditor.getText().indexOf('bar = function() {') === 0,
         'Correct source is shown');

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}
