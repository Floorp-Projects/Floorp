/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that updating the editor mode sets the right highlighting engine,
 * and source URIs with extra query parameters also get the right engine.
 */

const TAB_URL = EXAMPLE_URL + "doc_editor-mode.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources;

function test() {
  let options = {
    source: EXAMPLE_URL + "code_script-switching-01.js?a=b",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceAndCaretAndScopes(gPanel, "code_test-editor-mode", 1)
      .then(testInitialSource)
      .then(testSwitch1)
      .then(testSwitch2)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "firstCall");
  });
}

function testInitialSource() {
  is(gSources.itemCount, 3,
    "Found the expected number of sources.");

  is(gEditor.getMode().name, "text",
    "Found the expected editor mode.");
  is(gEditor.getText().search(/firstCall/), -1,
    "The first source is not displayed.");
  is(gEditor.getText().search(/debugger/), 135,
    "The second source is displayed.");
  is(gEditor.getText().search(/banana/), -1,
    "The third source is not displayed.");

  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
  gSources.selectedItem = e => e.attachment.label == "code_script-switching-01.js";
  return finished;
}

function testSwitch1() {
  is(gSources.itemCount, 3,
    "Found the expected number of sources.");

  is(gEditor.getMode().name, "javascript",
    "Found the expected editor mode.");
  is(gEditor.getText().search(/firstCall/), 118,
    "The first source is displayed.");
  is(gEditor.getText().search(/debugger/), -1,
    "The second source is not displayed.");
  is(gEditor.getText().search(/banana/), -1,
    "The third source is not displayed.");

  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
  gSources.selectedItem = e => e.attachment.label == "doc_editor-mode.html";
  return finished;
}

function testSwitch2() {
  is(gSources.itemCount, 3,
    "Found the expected number of sources.");

  is(gEditor.getMode().name, "htmlmixed",
    "Found the expected editor mode.");
  is(gEditor.getText().search(/firstCall/), -1,
    "The first source is not displayed.");
  is(gEditor.getText().search(/debugger/), -1,
    "The second source is not displayed.");
  is(gEditor.getText().search(/banana/), 443,
    "The third source is displayed.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
