/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helper code to play with the javascript thread
 **/

function getSandboxWithDebuggerSymbol() {
  // Bug 1835268 - Changing this to an ES module import currently throws an
  // assertion in test_javascript_tracer.js in debug builds.
  const { addDebuggerToGlobal } = ChromeUtils.import(
    "resource://gre/modules/jsdebugger.jsm"
  );
  const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

  const debuggerSandbox = Cu.Sandbox(systemPrincipal, {
    // This sandbox is also reused for ChromeDebugger implementation.
    // As we want to load the `Debugger` API for debugging chrome contexts,
    // we have to ensure loading it in a distinct compartment from its debuggee.
    freshCompartment: true,
    invisibleToDebugger: true,
  });
  addDebuggerToGlobal(debuggerSandbox);

  return debuggerSandbox;
}

/**
 * Implementation of a Javascript tracer logging traces to stdout.
 *
 * To be used like this:

  const { traceAllJSCalls } = ChromeUtils.importESModule(
    "resource://devtools/shared/test-helpers/thread-helpers.sys.mjs"
  );
  const jsTracer = traceAllJSCalls();
  [... execute some code to tracer ...]
  jsTracer.stop();

 * @param prefix String
 *        Optional, if passed, this will be displayed in front of each
 *        line reporting a new frame execution.
 * @param pause Number
 *        Optional, if passed, hold off each frame for `pause` ms,
 *        by letting the other event loops run in between.
 *        Be careful that it can introduce unexpected race conditions
 *        that can't necessarily be reproduced without this.
 */
export function traceAllJSCalls({ prefix = "", pause } = {}) {
  const debuggerSandbox = getSandboxWithDebuggerSymbol();

  debuggerSandbox.Services = Services;
  const f = Cu.evalInSandbox(
    "(" +
      function (pauseInMs, prefixString) {
        const dbg = new Debugger();
        // Add absolutely all the globals...
        dbg.addAllGlobalsAsDebuggees();
        // ...but avoid tracing this sandbox code
        const global = Cu.getGlobalForObject(this);
        dbg.removeDebuggee(global);

        // Add all globals created later on
        dbg.onNewGlobalObject = g => dbg.addDebuggee(g);

        function formatDisplayName(frame) {
          if (frame.type === "call") {
            const callee = frame.callee;
            return callee.name || callee.userDisplayName || callee.displayName;
          }

          return `(${frame.type})`;
        }

        function stop() {
          dbg.onEnterFrame = undefined;
          dbg.removeAllDebuggees();
        }
        global.stop = stop;

        let depth = 0;
        dbg.onEnterFrame = frame => {
          if (depth == 100) {
            dump(
              "Looks like an infinite loop? We stop the js tracer, but code may still be running!\n"
            );
            stop();
            return;
          }

          const { script } = frame;
          const { lineNumber, columnNumber } = script.getOffsetMetadata(
            frame.offset
          );
          const padding = new Array(depth).join(" ");
          dump(
            `${prefixString}${padding}--[${frame.implementation}]--> ${
              script.source.url
            } @ ${lineNumber}:${columnNumber} - ${formatDisplayName(frame)}\n`
          );

          depth++;
          frame.onPop = () => {
            depth--;
          };

          // Optionaly pause the frame execute by letting the other event loop to run in between.
          if (typeof pauseInMs == "number") {
            let freeze = true;
            const timer = Cc["@mozilla.org/timer;1"].createInstance(
              Ci.nsITimer
            );
            timer.initWithCallback(
              () => {
                freeze = false;
              },
              pauseInMs,
              Ci.nsITimer.TYPE_ONE_SHOT
            );
            Services.tm.spinEventLoopUntil("debugger-slow-motion", function () {
              return !freeze;
            });
          }
        };

        return { stop };
      } +
      ")",
    debuggerSandbox,
    undefined,
    "debugger-javascript-tracer",
    1,
    /* enforceFilenameRestrictions */ false
  );
  f(pause, prefix);

  return {
    stop() {
      debuggerSandbox.stop();
    },
  };
}
