/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can debug minified javascript with source maps.
 */

const TAB_URL = EXAMPLE_URL + "minified.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gClient = null;

let sourceShown = false;
let hitDebuggerStatement = false;

function test()
{
  SpecialPowers.pushPrefEnv({"set": [["devtools.debugger.source-maps-enabled", true]]}, () => {
    debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
      gTab = aTab;
      gDebuggee = aDebuggee;
      gPane = aPane;
      gDebugger = gPane.panelWin;
      gClient = gDebugger.DebuggerController.client;

      testMinJSAndSourceMaps();
    });
  });
}

function testMinJSAndSourceMaps() {
  gDebugger.addEventListener("Debugger:SourceShown", function _onSourceShown(aEvent) {
    gDebugger.removeEventListener("Debugger:SourceShown", _onSourceShown);
    sourceShown = true;

    ok(aEvent.detail.url.indexOf("math.min.js") == -1,
       "The debugger should not show the minified js");
    ok(aEvent.detail.url.indexOf("math.js") != -1,
       "The debugger should show the original js");
    ok(gDebugger.editor.getText().split("\n").length > 10,
       "The debugger's editor should have the original source displayed, " +
       "not the whitespace stripped minified version");

    startTest();
  });

  gClient.addListener("paused", function _onPaused(aEvent, aPacket) {
    if (aPacket.type === "paused" && aPacket.why.type === "breakpoint") {
      gClient.removeListener("paused", _onPaused);
      hitDebuggerStatement = true;

      startTest();
    }
  });

  gClient.activeThread.interrupt(function (aResponse) {
    ok(!aResponse.error, "Shouldn't be an error interrupting.");

    gClient.activeThread.setBreakpoint({
      url: EXAMPLE_URL + "math.js",
      line: 30,
      column: 10
    }, function (aResponse) {
      ok(!aResponse.error, "Shouldn't be an error setting a breakpoint.");
      ok(!aResponse.actualLocation, "Shouldn't be an actualLocation.");

      gClient.activeThread.resume(function (aResponse) {
        ok(!aResponse.error, "There shouldn't be an error resuming.");
        gDebuggee.arithmetic();
      });
    });
  });
}

function startTest() {
  if (sourceShown && hitDebuggerStatement) {
    testCaretPosition();
  }
}

function testCaretPosition() {
  waitForCaretPos(29, function () {
    closeDebuggerAndFinish();
  });
}

function waitForCaretPos(number, callback)
{
  // Poll every few milliseconds until the source editor line is active.
  let count = 0;
  let intervalID = window.setInterval(function() {
    info("count: " + count + ", caret at " + gDebugger.DebuggerView.editor.getCaretPosition().line);
    if (++count > 50) {
      ok(false, "Timed out while polling for the line.");
      window.clearInterval(intervalID);
      return closeDebuggerAndFinish();
    }
    if (gDebugger.DebuggerView.editor.getCaretPosition().line != number) {
      return;
    }
    // We got the source editor at the expected line, it's safe to callback.
    window.clearInterval(intervalID);
    callback();
  }, 100);
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gClient = null;
});
