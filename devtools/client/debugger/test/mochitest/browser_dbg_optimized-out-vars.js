/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that optimized out variables aren't present in the variables view.

function test() {
  Task.spawn(function* () {
    const TAB_URL = EXAMPLE_URL + "doc_closure-optimized-out.html";
    let gDebugger, sources;

    let options = {
      source: TAB_URL,
      line: 1
    };
    let [tab,, panel] = yield initDebugger(TAB_URL, options);
    gDebugger = panel.panelWin;
    sources = gDebugger.DebuggerView.Sources;

    yield panel.addBreakpoint({ actor: sources.values[0],
                                line: 18 });
    yield ensureThreadClientState(panel, "resumed");

    // Spin the event loop before causing the debuggee to pause, to allow
    // this function to return first.
    generateMouseClickInTab(tab, "content.document.querySelector('button')");

    yield waitForDebuggerEvents(panel, gDebugger.EVENTS.FETCHED_SCOPES);
    let gVars = gDebugger.DebuggerView.Variables;
    let outerScope = gVars.getScopeAtIndex(1);
    outerScope.expand();

    let upvarVar = outerScope.get("upvar");
    ok(upvarVar, "The variable `upvar` is shown.");
    is(upvarVar.target.querySelector(".value").getAttribute("value"),
       gDebugger.L10N.getStr("variablesViewOptimizedOut"),
       "Should show the optimized out message for upvar.");

    let argVar = outerScope.get("arg");
    is(argVar.target.querySelector(".name").getAttribute("value"), "arg",
      "Should have the right property name for |arg|.");
    is(argVar.target.querySelector(".value").getAttribute("value"), 42,
      "Should have the right property value for |arg|.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  }).then(null, aError => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  });
}
