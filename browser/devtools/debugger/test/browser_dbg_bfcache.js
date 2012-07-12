/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the debugger is updated with the correct scripts when moving
 * back and forward in the tab.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";
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

    testInitialLoad();
  });
}

function testInitialLoad() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    executeSoon(function() {
      validateFirstPage();
      testLocationChange();
    });
  });

  gDebuggee.firstCall();
}

function testLocationChange()
{
  gDebugger.DebuggerController.activeThread.resume(function() {
    gDebugger.DebuggerController.client.addOneTimeListener("tabAttached", function(aEvent, aPacket) {
      ok(true, "Successfully reattached to the tab again.");
      gDebugger.DebuggerController.client.addOneTimeListener("resumed", function(aEvent, aPacket) {
        executeSoon(function() {
          validateSecondPage();
          testBack();
        });
      });
    });
    content.location = STACK_URL;
  });
}

function testBack()
{
  gDebugger.DebuggerController.client.addOneTimeListener("tabAttached", function(aEvent, aPacket) {
    ok(true, "Successfully reattached to the tab after going back.");
    gDebugger.DebuggerController.client.addOneTimeListener("resumed", function(aEvent, aPacket) {
      executeSoon(function() {
        validateFirstPage();
        testForward();
      });
    });
  });

  info("Going back.");
  content.history.back();
}

function testForward()
{
  gDebugger.DebuggerController.client.addOneTimeListener("tabAttached", function(aEvent, aPacket) {
    ok(true, "Successfully reattached to the tab after going forward.");
    gDebugger.DebuggerController.client.addOneTimeListener("resumed", function(aEvent, aPacket) {
      executeSoon(function() {
        validateSecondPage();
        closeDebuggerAndFinish();
      });
    });
  });

  info("Going forward.");
  content.history.forward();
}

function validateFirstPage() {
  gScripts = gDebugger.DebuggerView.Scripts._scripts;

  is(gScripts.itemCount, 2, "Found the expected number of scripts.");

  let label1 = "test-script-switching-01.js";
  let label2 = "test-script-switching-02.js";

  ok(gDebugger.DebuggerView.Scripts.containsLabel(label1),
     "Found the first script label.");
  ok(gDebugger.DebuggerView.Scripts.containsLabel(label2),
     "Found the second script label.");
}

function validateSecondPage() {
  gScripts = gDebugger.DebuggerView.Scripts._scripts;

  is(gScripts.itemCount, 1, "Found the expected number of scripts.");

  ok(gDebugger.DebuggerView.Scripts.containsLabel("browser_dbg_stack.html"),
     "Found the single script label.");
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gScripts = null;
});
