"use strict";

/* global global, loadSubScript */

try {
  // For some reason WorkerDebuggerGlobalScope.global doesn't expose JS variables
  // and we can't call via global.foo(). Instead we have to go throught the Debugger API.
  const dbg = new Debugger(global);
  const [debuggee] = dbg.getDebuggees();

  /* global startTracing, stopTracing, addTracingListener, removeTracingListener */
  loadSubScript("resource://devtools/server/tracer/tracer.jsm");
  const frames = [];
  const listener = {
    onTracingFrame(args) {
      frames.push(args);

      // Return true, to also log the trace to stdout
      return true;
    },
  };
  addTracingListener(listener);
  startTracing({ global, prefix: "testWorkerPrefix" });

  debuggee.executeInGlobal("foo()");

  stopTracing();
  removeTracingListener(listener);

  // Send the frames to the main thread to do the assertions there.
  postMessage(JSON.stringify(frames));
} catch (e) {
  dump(
    "Exception while running debugger test script: " + e + "\n" + e.stack + "\n"
  );
}
