/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 978019: Setting a breakpoint on the last line of a Debugger.Script and
 * reloading should still hit the breakpoint.
 */

const TAB_URL = EXAMPLE_URL + "doc_breakpoints-break-on-last-line-of-script-on-reload.html";
const CODE_URL = EXAMPLE_URL + "code_breakpoints-break-on-last-line-of-script-on-reload.js";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  let gPanel, gDebugger, gThreadClient, gEvents, gSources;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gThreadClient = gDebugger.gThreadClient;
    gEvents = gDebugger.EVENTS;
    gSources = gDebugger.DebuggerView.Sources;
    const actions = bindActionCreators(gPanel);

    Task.spawn(function* () {
      try {

        // Refresh and hit the debugger statement before the location we want to
        // set our breakpoints. We have to pause before the breakpoint locations
        // so that GC doesn't get a chance to kick in and collect the IIFE's
        // script, which would causes us to receive a 'noScript' error from the
        // server when we try to set the breakpoints.
        const [paused, ] = yield promise.all([
          waitForThreadEvents(gPanel, "paused"),
          reloadActiveTab(gPanel, gEvents.SOURCE_SHOWN),
        ]);

        is(paused.why.type, "debuggerStatement");

        // Set our breakpoints.
        const sourceActor = getSourceActor(gSources, CODE_URL);
        yield promise.all([
          actions.addBreakpoint({
            actor: sourceActor,
            line: 3
          }),
          actions.addBreakpoint({
            actor: sourceActor,
            line: 4
          }),
          actions.addBreakpoint({
            actor: sourceActor,
            line: 5
          })
        ]);

        // Refresh and hit the debugger statement again.
        yield promise.all([
          reloadActiveTab(gPanel, gEvents.SOURCE_SHOWN),
          waitForCaretAndScopes(gPanel, 1)
        ]);

        // And we should hit the breakpoints as we resume.
        yield promise.all([
          doResume(gPanel),
          waitForCaretAndScopes(gPanel, 3)
        ]);
        yield promise.all([
          doResume(gPanel),
          waitForCaretAndScopes(gPanel, 4)
        ]);
        yield promise.all([
          doResume(gPanel),
          waitForCaretAndScopes(gPanel, 5)
        ]);

        // Clean up the breakpoints.
        yield promise.all([
          actions.removeBreakpoint({ actor: sourceActor, line: 3 }),
          actions.removeBreakpoint({ actor: sourceActor, line: 4 }),
          actions.removeBreakpoint({ actor: sourceActor, line: 5 })
        ]);

        yield resumeDebuggerThenCloseAndFinish(gPanel);

      } catch (e) {
        DevToolsUtils.reportException(
          "browser_dbg_breakpoints-break-on-last-line-of-script-on-reload.js",
          e
        );
        ok(false);
      }
    });
  });

  function setBreakpoint(location) {
    let item = gSources.getItemByValue(getSourceActor(gSources, location.url));
    let source = gThreadClient.source(item.attachment.source);

    let deferred = promise.defer();
    source.setBreakpoint(location, ({ error, message }, bpClient) => {
      if (error) {
        deferred.reject(error + ": " + message);
      }
      deferred.resolve(bpClient);
    });
    return deferred.promise;
  }
}
