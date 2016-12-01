/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 771452: Make sure that setting a breakpoint in an inline source doesn't
 * add it twice.
 */

const TAB_URL = EXAMPLE_URL + "doc_inline-script.html";

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require('./content/queries');
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    function testResume() {
      const deferred = promise.defer();
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

            deferred.resolve();
          });
        });

        generateMouseClickInTab(gTab, "content.document.querySelector('button')");
      });

      return deferred.promise;
    }

    function testBreakpointHit() {
      const deferred = promise.defer();
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

          deferred.resolve();
        });
      });

      EventUtils.sendMouseEvent({ type: "mousedown" },
                                gDebugger.document.getElementById("resume"),
                                gDebugger);
      return deferred.promise;
    }

    (async function(){
      let onCaretUpdated = waitForCaretAndScopes(gPanel, 16);
      callInTab(gTab, "runDebuggerStatement");
      await onCaretUpdated;

      is(gDebugger.gThreadClient.state, "paused",
         "The debugger statement was reached.");
      ok(isCaretPos(gPanel, 16),
         "The source editor caret position is incorrect (1).");

      await actions.addBreakpoint({ actor: getSourceActor(gSources, TAB_URL), line: 20 });
      await testResume();
      await testBreakpointHit();
      resumeDebuggerThenCloseAndFinish(gPanel);
    })();
  });
}
