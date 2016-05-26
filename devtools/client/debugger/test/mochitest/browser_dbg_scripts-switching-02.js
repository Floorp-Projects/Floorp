/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that switching the displayed source in the UI works as advertised.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-02.html";

var gLabel1 = "code_script-switching-01.js";
var gLabel2 = "code_script-switching-02.js";
var gParams = "?foo=bar,baz|lol";

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

    function testSourcesDisplay() {
      let deferred = promise.defer();

      is(gSources.itemCount, 2,
         "Found the expected number of sources. (1)");

      ok(getSourceActor(gSources, EXAMPLE_URL + gLabel1),
         "First source url is incorrect. (1)");
      ok(getSourceActor(gSources, EXAMPLE_URL + gLabel2 + gParams),
         "Second source url is incorrect. (1)");

      ok(gSources.getItemForAttachment(e => e.label == gLabel1),
         "First source label is incorrect. (1)");
      ok(gSources.getItemForAttachment(e => e.label == gLabel2),
         "Second source label is incorrect. (1)");

      ok(gSources.selectedItem,
         "There should be a selected item in the sources pane. (1)");
      is(getSelectedSourceURL(gSources), EXAMPLE_URL + gLabel2 + gParams,
         "The selected value is the sources pane is incorrect. (1)");

      is(gEditor.getText().search(/firstCall/), -1,
         "The first source is not displayed. (1)");
      is(gEditor.getText().search(/debugger/), 166,
         "The second source is displayed. (1)");

      ok(isCaretPos(gPanel, 6),
         "Editor caret location is correct. (1)");
      is(gEditor.getDebugLocation(), 5,
         "Editor debugger location is correct. (1)");
      ok(gEditor.hasLineClass(5, "debug-line"),
         "The debugged line is highlighted appropriately. (1)");

      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
      gSources.selectedItem = e => e.attachment.label == gLabel1;

      return deferred.promise;
    }

    function testSwitchPaused1() {
      let deferred = promise.defer();

      ok(gSources.selectedItem,
         "There should be a selected item in the sources pane. (2)");
      is(getSelectedSourceURL(gSources), EXAMPLE_URL + gLabel1,
         "The selected value is the sources pane is incorrect. (2)");

      is(gEditor.getText().search(/firstCall/), 118,
         "The first source is displayed. (2)");
      is(gEditor.getText().search(/debugger/), -1,
         "The second source is not displayed. (2)");

      // The editor's debug location takes a tick to update.
      ok(isCaretPos(gPanel, 1),
         "Editor caret location is correct. (2)");

      is(gEditor.getDebugLocation(), null,
         "Editor debugger location is correct. (2)");
      ok(!gEditor.hasLineClass(5, "debug-line"),
         "The debugged line highlight was removed. (2)");

      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
      gSources.selectedItem = e => e.attachment.label == gLabel2;

      return deferred.promise;
    }

    function testSwitchPaused2() {
      let deferred = promise.defer();

      ok(gSources.selectedItem,
         "There should be a selected item in the sources pane. (3)");
      is(getSelectedSourceURL(gSources), EXAMPLE_URL + gLabel2 + gParams,
         "The selected value is the sources pane is incorrect. (3)");

      is(gEditor.getText().search(/firstCall/), -1,
         "The first source is not displayed. (3)");
      is(gEditor.getText().search(/debugger/), 166,
         "The second source is displayed. (3)");

      // The editor's debug location takes a tick to update.
      ok(isCaretPos(gPanel, 6),
         "Editor caret location is correct. (3)");
      is(gEditor.getDebugLocation(), 5,
         "Editor debugger location is correct. (3)");
      ok(gEditor.hasLineClass(5, "debug-line"),
         "The debugged line is highlighted appropriately. (3)");

      // Step out three times.
      waitForThreadEvents(gPanel, "paused").then(() => {
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
        gDebugger.gThreadClient.stepOut();
      });
      gDebugger.gThreadClient.stepOut();

      return deferred.promise;
    }

    function testSwitchRunning() {
      let deferred = promise.defer();

      ok(gSources.selectedItem,
         "There should be a selected item in the sources pane. (4)");
      is(getSelectedSourceURL(gSources), EXAMPLE_URL + gLabel1,
         "The selected value is the sources pane is incorrect. (4)");

      is(gEditor.getText().search(/firstCall/), 118,
         "The first source is displayed. (4)");
      is(gEditor.getText().search(/debugger/), -1,
         "The second source is not displayed. (4)");

      // The editor's debug location takes a tick to update.
      ok(isCaretPos(gPanel, 6),
         "Editor caret location is correct. (4)");
      is(gEditor.getDebugLocation(), 5,
         "Editor debugger location is correct. (4)");
      ok(gEditor.hasLineClass(5, "debug-line"),
         "The debugged line is highlighted appropriately. (4)");

      deferred.resolve();

      return deferred.promise;
    }

    Task.spawn(function* () {
      yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1);
      yield testSourcesDisplay();
      yield testSwitchPaused1();
      yield testSwitchPaused2();
      yield testSwitchRunning();
      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    callInTab(gTab, "firstCall");
  });
}
