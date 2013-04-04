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
  let scriptShown = false;
  let framesAdded = false;

  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      let url = aEvent.detail.url;
      if (url.indexOf("browser_dbg_stack") != -1) {
        scriptShown = true;
        gDebugger.removeEventListener(aEvent.type, _onEvent);
        runTest();
      }
    });

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      runTest();
    });

    gDebuggee.simpleCall();
  });

  function runTest()
  {
    if (scriptShown && framesAdded) {
      Services.tm.currentThread.dispatch({ run: testSimpleCall }, 0);
    }
  }
}

function testSimpleCall() {
  var frames = gDebugger.DebuggerView.StackFrames._container._list,
      childNodes = frames.childNodes;

  is(gDebugger.DebuggerController.activeThread.state, "paused",
    "Should only be getting stack frames while paused.");

  is(frames.querySelectorAll(".dbg-stackframe").length, 1,
    "Should have only one frame.");

  is(childNodes.length, frames.querySelectorAll(".dbg-stackframe").length,
    "All children should be frames.");

  isnot(gDebugger.DebuggerView.Sources.selectedValue, null,
    "There should be a selected script.");
  isnot(gDebugger.editor.getText().length, 0,
    "The source editor should have some text displayed.");
  isnot(gDebugger.editor.getText(), gDebugger.L10N.getStr("loadingText"),
    "The source editor text should not be 'Loading...'");

  testLocationChange();
}

function testLocationChange()
{
  gDebugger.DebuggerController.activeThread.resume(function() {
    gDebugger.DebuggerController._target.once("navigate", function onTabNavigated(aEvent, aPacket) {
      ok(true, "tabNavigated event was fired.");
      info("Still attached to the tab.");

      gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);

        isnot(gDebugger.DebuggerView.Sources.selectedValue, null,
          "There should be a selected script.");
        isnot(gDebugger.editor.getText().length, 0,
          "The source editor should have some text displayed.");

        let menulist = gDebugger.DebuggerView.Sources._container;
        let noScripts = gDebugger.L10N.getStr("noSourcesText");
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
