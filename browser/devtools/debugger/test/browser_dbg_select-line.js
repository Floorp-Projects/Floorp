/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that selecting a stack frame loads the right script in the editor
 * pane and highlights the proper line.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let tempScope = {};
Cu.import("resource:///modules/source-editor.jsm", tempScope);
let SourceEditor = tempScope.SourceEditor;

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gScripts = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.contentWindow;

    testSelectLine();
  });
}

function testSelectLine() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("scriptsadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      gScripts = gDebugger.DebuggerView.Scripts._scripts;

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(gScripts.itemCount, 2, "Found the expected number of scripts.");

      ok(gDebugger.editor.getText().search(/debugger/) != -1,
        "The correct script was loaded initially.");

      // Yield control back to the event loop so that the debugger has a
      // chance to highlight the proper line.
      executeSoon(function(){
        // getCaretPosition is 0-based.
        is(gDebugger.editor.getCaretPosition().line, 5,
           "The correct line is selected.");

        gDebugger.editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                                          function onChange() {
          gDebugger.editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                                               onChange);
          ok(gDebugger.editor.getText().search(/debugger/) == -1,
            "The second script is no longer displayed.");

          ok(gDebugger.editor.getText().search(/firstCall/) != -1,
            "The first script is displayed.");

          // Yield control back to the event loop so that the debugger has a
          // chance to highlight the proper line.
          executeSoon(function(){
            // getCaretPosition is 0-based.
            is(gDebugger.editor.getCaretPosition().line, 4,
               "The correct line is selected.");

            gDebugger.DebuggerController.activeThread.resume(function() {
              closeDebuggerAndFinish();
            });
          });
        });

        // Scroll all the way down to ensure stackframe-3 is visible.
        let stackframes = gDebugger.document.getElementById("stackframes");
        stackframes.scrollTop = stackframes.scrollHeight;

        // Click the oldest stack frame.
        let frames = gDebugger.DebuggerView.Stackframes._frames;
        is(frames.querySelectorAll(".dbg-stackframe").length, 4,
          "Should have four frames.");

        let element = gDebugger.document.getElementById("stackframe-3");
        isnot(element, null, "Found the third stack frame.");
        EventUtils.synthesizeMouseAtCenter(element, {}, gDebugger);
      });
    }}, 0);
  });

  gDebuggee.firstCall();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
