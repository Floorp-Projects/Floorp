/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 771452: Make sure that setting a breakpoint in an inline source doesn't
 * add it twice.
 */

const TAB_URL = EXAMPLE_URL + "doc_inline-script.html";

let gTab, gPanel, gDebugger, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    addBreakpoint();
  });
}

function addBreakpoint() {
  waitForSourceAndCaretAndScopes(gPanel, ".html", 16).then(() => {
    is(gDebugger.gThreadClient.state, "paused",
      "The debugger statement was reached.");
    ok(isCaretPos(gPanel, 16),
      "The source editor caret position is incorrect (1).");

    gPanel.addBreakpoint({ actor: getSourceActor(gSources, TAB_URL), line: 20 }).then(() => {
      testResume();
    });
  });

  callInTab(gTab, "runDebuggerStatement");
}

function testResume() {
  is(gDebugger.gThreadClient.state, "paused",
    "The breakpoint wasn't hit yet.");

  gDebugger.gThreadClient.resume(() => {
    gDebugger.gThreadClient.addOneTimeListener("paused", (aEvent, aPacket) => {
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
        is(aPacket.why.type, "breakpoint",
          "Execution has advanced to the next breakpoint.");
        isnot(aPacket.why.type, "debuggerStatement",
          "The breakpoint was hit before the debugger statement.");
        ok(isCaretPos(gPanel, 20),
          "The source editor caret position is incorrect (2).");

        testBreakpointHit();
      });
    });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function testBreakpointHit() {
  is(gDebugger.gThreadClient.state, "paused",
    "The breakpoint was hit.");

  gDebugger.gThreadClient.addOneTimeListener("paused", (aEvent, aPacket) => {
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
      is(aPacket.why.type, "debuggerStatement",
        "Execution has advanced to the next line.");
      isnot(aPacket.why.type, "breakpoint",
        "No ghost breakpoint was hit.");
      ok(isCaretPos(gPanel, 20),
        "The source editor caret position is incorrect (3).");

      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
});
