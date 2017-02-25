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

add_task(function* () {
  // Ensure the profiler is already running when the test starts.
  pmmLoadFrameScripts(gBrowser);
  let entries = 1000000;
  let interval = 1;
  let features = ["js"];
  yield pmmStartProfiler({ entries, interval, features });

  ok((yield pmmIsProfilerActive()),
    "The built-in profiler module should still be active.");

  yield addTab(MAIN_DOMAIN + "doc_perf.html");
  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let firstFront = PerformanceFront(client, form);
  yield firstFront.connect();

  yield firstFront.startRecording();

  yield addTab(MAIN_DOMAIN + "doc_perf.html");
  let client2 = new DebuggerClient(DebuggerServer.connectPipe());
  let form2 = yield connectDebuggerClient(client2);
  let secondFront = PerformanceFront(client2, form2);
  yield secondFront.connect();

  yield secondFront.destroy();
  yield client2.close();
  ok((yield pmmIsProfilerActive()),
    "The built-in profiler module should still be active.");

  yield firstFront.destroy();
  yield client.close();
  ok(!(yield pmmIsProfilerActive()),
    "The built-in profiler module should have been automatically stopped.");

  pmmClearFrameScripts();

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
