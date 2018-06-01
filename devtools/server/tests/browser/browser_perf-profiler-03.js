/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the built-in profiler module is not reactivated if no other
 * consumer was using it over the remote debugger protocol, and ensures
 * that the actor will work properly even in such cases (e.g. the Gecko Profiler
 * addon was installed and automatically activated the profiler module).
 */

"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");
const { pmmIsProfilerActive, pmmStartProfiler, pmmLoadFrameScripts, pmmClearFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");

add_task(async function() {
  // Ensure the profiler is already running when the test starts.
  pmmLoadFrameScripts(gBrowser);
  const entries = 1000000;
  const interval = 1;
  const features = ["js"];
  await pmmStartProfiler({ entries, interval, features });

  ok((await pmmIsProfilerActive()),
    "The built-in profiler module should still be active.");

  await addTab(MAIN_DOMAIN + "doc_perf.html");
  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const firstFront = PerformanceFront(client, form);
  await firstFront.connect();

  await firstFront.startRecording();

  await addTab(MAIN_DOMAIN + "doc_perf.html");
  const client2 = new DebuggerClient(DebuggerServer.connectPipe());
  const form2 = await connectDebuggerClient(client2);
  const secondFront = PerformanceFront(client2, form2);
  await secondFront.connect();

  await secondFront.destroy();
  await client2.close();
  ok((await pmmIsProfilerActive()),
    "The built-in profiler module should still be active.");

  await firstFront.destroy();
  await client.close();
  ok(!(await pmmIsProfilerActive()),
    "The built-in profiler module should have been automatically stopped.");

  pmmClearFrameScripts();

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
