"use strict";

/* global global */

try {
  // For some reason WorkerDebuggerGlobalScope.global doesn't expose JS variables
  // and we can't call via global.foo(). Instead we have to go throught the Debugger API.
  const dbg = new Debugger(global);
  const [debuggee] = dbg.getDebuggees();

  const { JSTracer } = ChromeUtils.importESModule(
    "resource://devtools/server/tracer/tracer.sys.mjs",
    { global: "contextual" }
  );

  const frames = [];
  const listener = {
    onTracingFrame(args) {
      frames.push(args);

      // Return true, to also log the trace to stdout
      return true;
    },
  };
  JSTracer.addTracingListener(listener);
  JSTracer.startTracing({ global, prefix: "testWorkerPrefix" });

  debuggee.executeInGlobal("foo()");

  JSTracer.stopTracing();
  JSTracer.removeTracingListener(listener);

  // Send the frames to the main thread to do the assertions there.
  postMessage(JSON.stringify(frames));
} catch (e) {
  dump(
    "Exception while running debugger test script: " + e + "\n" + e.stack + "\n"
  );
}
