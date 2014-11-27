/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can toggle between the original and generated sources.
 */

const TAB_URL = EXAMPLE_URL + "doc_binary_search.html";
const JS_URL = EXAMPLE_URL + "code_binary_search.js";

let gTab, gPanel, gDebugger, gEditor;
let gSources, gFrames, gPrefs, gOptions;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gPrefs = gDebugger.Prefs;
    gOptions = gDebugger.DebuggerView.Options;

    waitForSourceShown(gPanel, ".coffee")
      .then(testToggleGeneratedSource)
      .then(testSetBreakpoint)
      .then(testToggleOnPause)
      .then(testResume)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testToggleGeneratedSource() {
  let finished = waitForSourceShown(gPanel, ".js").then(() => {
    is(gPrefs.sourceMapsEnabled, false,
      "The source maps pref should have been set to false.");
    is(gOptions._showOriginalSourceItem.getAttribute("checked"), "false",
      "Source maps should now be disabled.")

    is(gSources.selectedItem.attachment.source.url.indexOf(".coffee"), -1,
      "The debugger should not show the source mapped coffee source file.");
    isnot(gSources.selectedItem.attachment.source.url.indexOf(".js"), -1,
      "The debugger should show the generated js source file.");

    is(gEditor.getText().indexOf("isnt"), -1,
      "The debugger's editor should not have the coffee source source displayed.");
    is(gEditor.getText().indexOf("function"), 36,
      "The debugger's editor should have the JS source displayed.");
  });

  gOptions._showOriginalSourceItem.setAttribute("checked", "false");
  gOptions._toggleShowOriginalSource();
  gOptions._onPopupHidden();

  return finished;
}

function testSetBreakpoint() {
  let deferred = promise.defer();
  let sourceForm = getSourceForm(gSources, JS_URL);
  let source = gDebugger.gThreadClient.source(sourceForm);

  source.setBreakpoint({ line: 7 }, aResponse => {
    ok(!aResponse.error,
      "Should be able to set a breakpoint in a js file.");

    gDebugger.gClient.addOneTimeListener("resumed", () => {
      waitForCaretAndScopes(gPanel, 7).then(() => {
        // Make sure that we have JavaScript stack frames.
        is(gFrames.itemCount, 1,
          "Should have only one frame.");
        is(gFrames.getItemAtIndex(0).attachment.url.indexOf(".coffee"), -1,
          "First frame should not be a coffee source frame.");
        isnot(gFrames.getItemAtIndex(0).attachment.url.indexOf(".js"), -1,
          "First frame should be a JS frame.");

        deferred.resolve();
      });

      // This will cause the breakpoint to be hit, and put us back in the
      // paused state.
      callInTab(gTab, "binary_search", [0, 2, 3, 5, 7, 10], 5);
    });
  });

  return deferred.promise;
}

function testToggleOnPause() {
  let finished = waitForSourceAndCaretAndScopes(gPanel, ".coffee", 5).then(() => {
    is(gPrefs.sourceMapsEnabled, true,
      "The source maps pref should have been set to true.");
    is(gOptions._showOriginalSourceItem.getAttribute("checked"), "true",
      "Source maps should now be enabled.")

    isnot(gSources.selectedItem.attachment.source.url.indexOf(".coffee"), -1,
      "The debugger should show the source mapped coffee source file.");
    is(gSources.selectedItem.attachment.source.url.indexOf(".js"), -1,
      "The debugger should not show the generated js source file.");

    is(gEditor.getText().indexOf("isnt"), 218,
      "The debugger's editor should have the coffee source source displayed.");
    is(gEditor.getText().indexOf("function"), -1,
      "The debugger's editor should not have the JS source displayed.");

    // Make sure that we have coffee source stack frames.
    is(gFrames.itemCount, 1,
      "Should have only one frame.");
    is(gFrames.getItemAtIndex(0).attachment.url.indexOf(".js"), -1,
      "First frame should not be a JS frame.");
    isnot(gFrames.getItemAtIndex(0).attachment.url.indexOf(".coffee"), -1,
      "First frame should be a coffee source frame.");
  });

  gOptions._showOriginalSourceItem.setAttribute("checked", "true");
  gOptions._toggleShowOriginalSource();
  gOptions._onPopupHidden();

  return finished;
}

function testResume() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.resume(aResponse => {
    ok(!aResponse.error, "Shouldn't get an error resuming.");
    is(aResponse.type, "resumed", "Type should be 'resumed'.");

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
  gFrames = null;
  gPrefs = null;
  gOptions = null;
});
