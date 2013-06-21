/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if breakpoints are highlighted when they should.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gEditor = null;
let gSources = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gSources.preferredSource = EXAMPLE_URL + "test-script-switching-02.js";

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      let url = aEvent.detail.url;
      if (url.indexOf("-02") != -1) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);
        performTest();
      }
    });
  });

  function performTest()
  {
    initialChecks();
    gPane.addBreakpoint({url: gSources.values[0], line: 5}, function(cl, err) {
      initialChecks();
      gPane.addBreakpoint({url: gSources.values[1], line: 6}, function(cl, err) {
        initialChecks();
        gPane.addBreakpoint({url: gSources.values[1], line: 7}, function(cl, err) {
          initialChecks();
          gPane.addBreakpoint({url: gSources.values[1], line: 8}, function(cl, err) {
            initialChecks();
            gPane.addBreakpoint({url: gSources.values[1], line: 9}, function(cl, err) {
              initialChecks();
              testHighlight1();
            });
          });
        });
      });
    });
  }

  function initialChecks() {
    is(gSources.selectedValue, gSources.values[1],
      "The currently selected source is incorrect (0).");
    is(gEditor.getCaretPosition().line, 0,
      "The editor caret line was incorrect (0).");
    is(gEditor.getCaretPosition().col, 0,
      "The editor caret column was incorrect (0).");
  }

  function testHighlight1() {
    gSources.highlightBreakpoint(gSources.values[0], 5);
    checkHighlight(gSources.values[0], 5);

    is(gSources.selectedValue, gSources.values[1],
      "The currently selected source is incorrect (1).");

    is(gEditor.getCaretPosition().line, 0,
      "The editor caret line was incorrect (1).");
    is(gEditor.getCaretPosition().col, 0,
      "The editor caret column was incorrect (1).");

    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelectorAll(".dbg-breakpoint")[0],
      gDebugger);

    waitForCaretPos(4, function() {
      ok(true, "The editor location is correct (1).");
      testHighlight2();
    });
  }

  function testHighlight2() {
    gSources.highlightBreakpoint(gSources.values[1], 6);
    checkHighlight(gSources.values[1], 6);

    is(gSources.selectedValue, gSources.values[0],
      "The currently selected source is incorrect (2).");

    is(gEditor.getCaretPosition().line, 4,
      "The editor caret line was incorrect (2).");
    is(gEditor.getCaretPosition().col, 0,
      "The editor caret column was incorrect (2).");

    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelectorAll(".dbg-breakpoint")[1],
      gDebugger);

    waitForCaretPos(5, function() {
      ok(true, "The editor location is correct (2).");
      testHighlight3();
    });
  }

  function testHighlight3() {
    gSources.highlightBreakpoint(gSources.values[1], 7);
    checkHighlight(gSources.values[1], 7);

    is(gSources.selectedValue, gSources.values[1],
      "The currently selected source is incorrect (3).");

    is(gEditor.getCaretPosition().line, 5,
      "The editor caret line was incorrect (3).");
    is(gEditor.getCaretPosition().col, 0,
      "The editor caret column was incorrect (3).");

    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelectorAll(".dbg-breakpoint")[2],
      gDebugger);

    waitForCaretPos(6, function() {
      ok(true, "The editor location is correct (3).");
      testHighlight4();
    });
  }

  function testHighlight4() {
    gSources.highlightBreakpoint(gSources.values[1], 8);
    checkHighlight(gSources.values[1], 8);

    is(gSources.selectedValue, gSources.values[1],
      "The currently selected source is incorrect (4).");

    is(gEditor.getCaretPosition().line, 6,
      "The editor caret line was incorrect (4).");
    is(gEditor.getCaretPosition().col, 0,
      "The editor caret column was incorrect (4).");

    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelectorAll(".dbg-breakpoint")[3],
      gDebugger);

    waitForCaretPos(7, function() {
      ok(true, "The editor location is correct (4).");
      testHighlight5();
    });
  }

  function testHighlight5() {
    gSources.highlightBreakpoint(gSources.values[1], 9);
    checkHighlight(gSources.values[1], 9);

    is(gSources.selectedValue, gSources.values[1],
      "The currently selected source is incorrect (5).");

    is(gEditor.getCaretPosition().line, 7,
      "The editor caret line was incorrect (5).");
    is(gEditor.getCaretPosition().col, 0,
      "The editor caret column was incorrect (5).");

    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelectorAll(".dbg-breakpoint")[4],
      gDebugger);

    waitForCaretPos(8, function() {
      ok(true, "The editor location is correct (5).");
      closeDebuggerAndFinish();
    });
  }

  function checkHighlight(aUrl, aLine) {
    is(gSources.selectedBreakpointItem, gSources.getBreakpoint(aUrl, aLine),
      "The currently selected breakpoint item is incorrect.");
    is(gSources.selectedBreakpointClient, gPane.getBreakpoint(aUrl, aLine),
      "The currently selected breakpoint client is incorrect.");

    is(gSources.selectedBreakpointItem.attachment.sourceLocation, aUrl,
      "The selected breakpoint item's source location attachment is incorrect.");
    is(gSources.selectedBreakpointItem.attachment.lineNumber, aLine,
      "The selected breakpoint item's source line number is incorrect.");

    ok(gSources.selectedBreakpointItem.target.classList.contains("selected"),
      "The selected breakpoint item's target should have a selected class.");
  }

  function waitForCaretPos(number, callback)
  {
    // Poll every few milliseconds until the source editor line is active.
    let count = 0;
    let intervalID = window.setInterval(function() {
      info("count: " + count + " ");
      if (++count > 50) {
        ok(false, "Timed out while polling for the line.");
        window.clearInterval(intervalID);
        return closeDebuggerAndFinish();
      }
      if (gEditor.getCaretPosition().line != number) {
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
    gEditor = null;
    gSources = null;
  });
}
