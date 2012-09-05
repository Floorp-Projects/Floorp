/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL to a page with other scripts works.
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
      info("Still attached to the tab.");

      gDebugger.addEventListener("Debugger:AfterNewScript", function _onEvent(aEvent) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);

        isnot(gDebugger.DebuggerView.Scripts.selected, null,
          "There should be a selected script.");
        isnot(gDebugger.editor.getText().length, 0,
          "The source editor should have some text displayed.");

        let menulist = gDebugger.DebuggerView.Scripts._scripts;
        let noScripts = gDebugger.L10N.getStr("noScriptsText");
        isnot(menulist.getAttribute("label"), noScripts,
          "The menulist should not display a notice that there are no scripts availalble.");
        isnot(menulist.getAttribute("tooltiptext"), "",
          "The menulist should have a tooltip text attributed.");

        closeDebuggerAndFinish();
      });
    });
    content.location = EXAMPLE_URL + "browser_dbg_iframes.html";
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
