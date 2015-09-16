/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that switching the displayed source in the UI works as advertised.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    ok(gDebugger.document.title.endsWith(EXAMPLE_URL + gLabel1),
      "Title with first source is correct.");

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(testSourcesDisplay)
      .then(testSwitchPaused1)
      .then(testSwitchPaused2)
      .then(testSwitchRunning)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "firstCall");
  });
}

var gLabel1 = "code_script-switching-01.js";
var gLabel2 = "code_script-switching-02.js";

function testSourcesDisplay() {
  let deferred = promise.defer();

  is(gSources.itemCount, 2,
    "Found the expected number of sources.");

  is(gSources.items[0].target.querySelector(".dbg-source-item").getAttribute("tooltiptext"),
    EXAMPLE_URL + "code_script-switching-01.js",
    "The correct tooltip text is displayed for the first source.");
  is(gSources.items[1].target.querySelector(".dbg-source-item").getAttribute("tooltiptext"),
    EXAMPLE_URL + "code_script-switching-02.js",
    "The correct tooltip text is displayed for the second source.");

  ok(getSourceActor(gSources, EXAMPLE_URL + gLabel1),
    "First source url is incorrect.");
  ok(getSourceActor(gSources, EXAMPLE_URL + gLabel2),
    "Second source url is incorrect.");

  ok(gSources.getItemForAttachment(e => e.label == gLabel1),
    "First source label is incorrect.");
  ok(gSources.getItemForAttachment(e => e.label == gLabel2),
    "Second source label is incorrect.");

  ok(gSources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(getSelectedSourceURL(gSources), EXAMPLE_URL + gLabel2,
    "The selected value is the sources pane is incorrect.");

  is(gEditor.getText().search(/firstCall/), -1,
    "The first source is not displayed.");
  is(gEditor.getText().search(/debugger/), 166,
    "The second source is displayed.");

  ok(gDebugger.document.title.endsWith(EXAMPLE_URL + gLabel2),
    "Title with second source is correct.");

  ok(isCaretPos(gPanel, 1),
    "Editor caret location is correct.");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    is(gEditor.getDebugLocation(), 5,
      "Editor debugger location is correct.");
    ok(gEditor.hasLineClass(5, "debug-line"),
      "The debugged line is highlighted appropriately (1).");

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
    gSources.selectedIndex = 0;
  });

  return deferred.promise;
}

function testSwitchPaused1() {
  let deferred = promise.defer();

  ok(gSources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(getSelectedSourceURL(gSources), EXAMPLE_URL + gLabel1,
    "The selected value is the sources pane is incorrect.");

  is(gEditor.getText().search(/firstCall/), 118,
    "The first source is displayed.");
  is(gEditor.getText().search(/debugger/), -1,
    "The second source is not displayed.");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    ok(isCaretPos(gPanel, 1),
      "Editor caret location is correct.");
    is(gEditor.getDebugLocation(), null,
      "Editor debugger location is correct.");
    ok(!gEditor.hasLineClass(5, "debug-line"),
      "The debugged line highlight was removed.");

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
    gSources.selectedIndex = 1;
  });

  return deferred.promise;
}

function testSwitchPaused2() {
  let deferred = promise.defer();

  ok(gSources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(getSelectedSourceURL(gSources), EXAMPLE_URL + gLabel2,
    "The selected value is the sources pane is incorrect.");

  is(gEditor.getText().search(/firstCall/), -1,
    "The first source is not displayed.");
  is(gEditor.getText().search(/debugger/), 166,
    "The second source is displayed.");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    ok(isCaretPos(gPanel, 6),
      "Editor caret location is correct.");
    is(gEditor.getDebugLocation(), 5,
      "Editor debugger location is correct.");
    ok(gEditor.hasLineClass(5, "debug-line"),
      "The debugged line is highlighted appropriately (2).");

    // Step out twice.
    waitForThreadEvents(gPanel, "paused").then(() => {
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
      gDebugger.gThreadClient.stepOut();
    })
    gDebugger.gThreadClient.stepOut();
  });

  return deferred.promise;
}

function testSwitchRunning() {
  let deferred = promise.defer();

  ok(gSources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(getSelectedSourceURL(gSources), EXAMPLE_URL + gLabel1,
    "The selected value is the sources pane is incorrect.");

  is(gEditor.getText().search(/firstCall/), 118,
    "The first source is displayed.");
  is(gEditor.getText().search(/debugger/), -1,
    "The second source is not displayed.");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    ok(isCaretPos(gPanel, 5),
      "Editor caret location is correct.");
    is(gEditor.getDebugLocation(), 4,
      "Editor debugger location is correct.");
    ok(gEditor.hasLineClass(4, "debug-line"),
      "The debugged line is highlighted appropriately (3).");

    deferred.resolve();
  });

  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gLabel1 = null;
  gLabel2 = null;
});
