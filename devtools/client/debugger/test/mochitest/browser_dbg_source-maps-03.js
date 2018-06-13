/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can debug minified javascript with source maps.
 */

const TAB_URL = EXAMPLE_URL + "doc_minified.html";
const JS_URL = EXAMPLE_URL + "code_math.js";

var gTab, gPanel, gDebugger;
var gEditor, gSources, gFrames;

function test() {
  let options = {
    source: JS_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;

    checkInitialSource()
    testSetBreakpoint()
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function checkInitialSource() {
  isnot(gSources.selectedItem.attachment.source.url.indexOf(".js"), -1,
    "The debugger should not show the minified js file.");
  is(gSources.selectedItem.attachment.source.url.indexOf(".min.js"), -1,
    "The debugger should show the original js file.");
  is(gEditor.getText().split("\n").length, 46,
    "The debugger's editor should have the original source displayed, " +
    "not the whitespace stripped minified version.");
}

function testSetBreakpoint() {
  let deferred = promise.defer();
  let sourceForm = getSourceForm(gSources, JS_URL);
  let source = gDebugger.gThreadClient.source(sourceForm);

  source.setBreakpoint({ line: 30 }).then(([aResponse]) => {
    ok(!aResponse.actualLocation,
      "Should be able to set a breakpoint on line 30.");

    gDebugger.gClient.addOneTimeListener("resumed", () => {
      waitForCaretAndScopes(gPanel, 30).then(() => {
        // Make sure that we have the right stack frames.
        is(gFrames.itemCount, 9,
          "Should have nine frames.");
        is(gFrames.getItemAtIndex(0).attachment.url.indexOf(".min.js"), -1,
          "First frame should not be a minified JS frame.");
        isnot(gFrames.getItemAtIndex(0).attachment.url.indexOf(".js"), -1,
          "First frame should be a JS frame.");

        deferred.resolve();
      });

      // This will cause the breakpoint to be hit, and put us back in the
      // paused state.
      callInTab(gTab, "arithmetic");
    });
  });

  return deferred.promise;
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gFrames = null;
});
