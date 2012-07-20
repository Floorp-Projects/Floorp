/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL to a page with no scripts works.
 */

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test()
{
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.contentWindow;

    testSimpleCall();
  });
}

function testSimpleCall() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({
      run: function() {
        var frames = gDebugger.DebuggerView.StackFrames._frames,
            childNodes = frames.childNodes;

        is(gDebugger.DebuggerController.activeThread.state, "paused",
          "Should only be getting stack frames while paused.");

        is(frames.querySelectorAll(".dbg-stackframe").length, 1,
          "Should have only one frame.");

        is(childNodes.length, frames.querySelectorAll(".dbg-stackframe").length,
          "All children should be frames.");

        isnot(gDebugger.DebuggerView.Scripts.selected, null,
          "There should be a selected script.");
        isnot(gDebugger.editor.getText().length, 0,
          "The source editor should have some text displayed.");

        testLocationChange();
      }
    }, 0);
  });

  gDebuggee.simpleCall();
}

function testLocationChange()
{
  gDebugger.DebuggerController.activeThread.resume(function() {
    gDebugger.DebuggerController.client.addOneTimeListener("tabNavigated", function(aEvent, aPacket) {
      ok(true, "tabNavigated event was fired.");
      gDebugger.DebuggerController.client.addOneTimeListener("tabAttached", function(aEvent, aPacket) {
        ok(true, "Successfully reattached to the tab again.");

        // Wait for the initial resume...
        gDebugger.gClient.addOneTimeListener("resumed", function() {
          is(gDebugger.DebuggerView.Scripts.selected, null,
            "There should be no selected script.");
          is(gDebugger.editor.getText().length, 0,
            "The source editor not have any text displayed.");

          let menulist = gDebugger.DebuggerView.Scripts._scripts;
          let noScripts = gDebugger.L10N.getStr("noScriptsText");
          is(menulist.getAttribute("label"), noScripts,
            "The menulist should display a notice that there are no scripts availalble.");
          is(menulist.getAttribute("tooltiptext"), "",
            "The menulist shouldn't have any tooltip text attributed when there are no scripts available.");

          closeDebuggerAndFinish();
        });
      });
    });
    content.location = "about:blank";
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
