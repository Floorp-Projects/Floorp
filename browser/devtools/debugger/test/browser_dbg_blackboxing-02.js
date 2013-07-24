/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that black boxed frames are compressed into a single frame on the stack
 * view.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_blackboxing.html";
const BLACKBOXME_URL = EXAMPLE_URL + "blackboxing_blackboxme.js"

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test()
{
  let scriptShown = false;
  let framesAdded = false;
  let resumed = false;
  let testStarted = false;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    resumed = true;
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    testBlackBoxSource();
  });
}

function testBlackBoxSource() {
  once(gDebugger, "Debugger:SourceShown", function () {
    const checkbox = getBlackBoxCheckbox(BLACKBOXME_URL);
    ok(checkbox, "Should get the checkbox for blackBoxing the source");

    once(gDebugger, "Debugger:BlackBoxChange", function (event) {
      const sourceClient = event.detail;
      ok(sourceClient.isBlackBoxed, "The source should be black boxed now");

      testBlackBoxStack();
    });

    checkbox.click();
  });
}

function testBlackBoxStack() {
  const { activeThread } = gDebugger.DebuggerController;
  activeThread.addOneTimeListener("framesadded", function () {
    const frames = gDebugger.DebuggerView.StackFrames.widget._list;

    is(frames.querySelectorAll(".dbg-stackframe").length, 3,
       "Should only get 3 frames");

    is(frames.querySelectorAll(".dbg-stackframe-black-boxed").length, 1,
       "And one of them should be the combined black boxed frames");

    closeDebuggerAndFinish();
  });

  gDebuggee.runTest();
}

function getBlackBoxCheckbox(url) {
  return gDebugger.document.querySelector(
    ".side-menu-widget-item[tooltiptext=\""
      + url + "\"] .side-menu-widget-item-checkbox");
}

function once(target, event, callback) {
  target.addEventListener(event, function _listener(...args) {
    target.removeEventListener(event, _listener, false);
    callback.apply(null, args);
  }, false);
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
