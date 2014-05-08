/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that optimized out variables aren't present in the variables view.

function test() {
  Task.spawn(function* () {
    const TAB_URL = EXAMPLE_URL + "doc_closure-optimized-out.html";
    let panel, debuggee, gDebugger, sources;

    let [, debuggee, panel] = yield initDebugger(TAB_URL);
    gDebugger = panel.panelWin;
    sources = gDebugger.DebuggerView.Sources;

    yield waitForSourceShown(panel, ".html");
    yield panel.addBreakpoint({ url: sources.values[0], line: 18 });
    yield ensureThreadClientState(panel, "resumed");

    // Spin the event loop before causing the debuggee to pause, to allow
    // this function to return first.
    executeSoon(() => {
      EventUtils.sendMouseEvent({ type: "click" },
        debuggee.document.querySelector("button"),
        debuggee);
    });

    yield waitForDebuggerEvents(panel, gDebugger.EVENTS.FETCHED_SCOPES);
    let gVars = gDebugger.DebuggerView.Variables;
    let outerScope = gVars.getScopeAtIndex(1);
    outerScope.expand();

    let upvarVar = outerScope.get("upvar");
    ok(!upvarVar, "upvar was optimized out.");
    if (upvarVar) {
      ok(false, "upvar = " + upvarVar.target.querySelector(".value").getAttribute("value"));
    }

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
