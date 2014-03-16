/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 978019: Setting a breakpoint on the last line of a Debugger.Script and
 * reloading should still hit the breakpoint.
 */

const TAB_URL = EXAMPLE_URL + "doc_breakpoints-break-on-last-line-of-script-on-reload.html";
const CODE_URL = EXAMPLE_URL + "code_breakpoints-break-on-last-line-of-script-on-reload.js";

const { promiseInvoke } = require("devtools/async-utils");

function test() {
  let gPanel, gDebugger, gThreadClient, gEvents;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gThreadClient = gDebugger.gThreadClient;
    gEvents = gDebugger.EVENTS;

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
        const [bp1, bp2, bp3] = yield promise.all([
          setBreakpoint({
            url: CODE_URL,
            line: 3
          }),
          setBreakpoint({
            url: CODE_URL,
            line: 4
          }),
          setBreakpoint({
            url: CODE_URL,
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
          doResume(),
          waitForCaretAndScopes(gPanel, 3)
        ]);
        yield promise.all([
          doResume(),
          waitForCaretAndScopes(gPanel, 4)
        ]);
        yield promise.all([
          doResume(),
          waitForCaretAndScopes(gPanel, 5)
        ]);

        // Clean up the breakpoints.
        yield promise.all([
          rdpInvoke(bp1, bp1.remove),
          rdpInvoke(bp2, bp1.remove),
          rdpInvoke(bp3, bp1.remove),
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

  function rdpInvoke(obj, method) {
    return promiseInvoke(obj, method)
      .then(({error, message }) => {
        if (error) {
          throw new Error(error + ": " + message);
        }
      });
  }

  function doResume() {
    return rdpInvoke(gThreadClient, gThreadClient.resume);
  }

  function doInterrupt() {
    return rdpInvoke(gThreadClient, gThreadClient.interrupt);
  }

  function setBreakpoint(location) {
    let deferred = promise.defer();
    gThreadClient.setBreakpoint(location, ({ error, message }, bpClient) => {
      if (error) {
        deferred.reject(error + ": " + message);
      }
      deferred.resolve(bpClient);
    });
    return deferred.promise;
  }
}
