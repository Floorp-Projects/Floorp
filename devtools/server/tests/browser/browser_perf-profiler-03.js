/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the built-in profiler module is not reactivated if no other
 * consumer was using it over the remote debugger protocol, and ensures
 * that the actor will work properly even in such cases (e.g. the Gecko Profiler
 * addon was installed and automatically activated the profiler module).
 */

const { PerformanceFront } = require("devtools/server/actors/performance");
const { sendProfilerCommand, PMM_isProfilerActive, PMM_stopProfiler, PMM_loadFrameScripts } = require("devtools/shared/performance/process-communication");

add_task(function*() {
  // Ensure the profiler is already running when the test starts.
  PMM_loadFrameScripts(gBrowser);
  let ENTRIES = 1000000;
  let INTERVAL = 1;
  let FEATURES = ["js"];
  yield sendProfilerCommand("StartProfiler", [ENTRIES, INTERVAL, FEATURES, FEATURES.length]);

  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should still be active.");

  yield addTab(MAIN_DOMAIN + "doc_perf.html");
  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let firstFront = PerformanceFront(client, form);
  yield firstFront.connect();

  let recording = yield firstFront.startRecording();

  yield addTab(MAIN_DOMAIN + "doc_perf.html");
  let client2 = new DebuggerClient(DebuggerServer.connectPipe());
  let form2 = yield connectDebuggerClient(client2);
  let secondFront = PerformanceFront(client2, form2);
  yield secondFront.connect();

  yield secondFront.destroy();
  yield closeDebuggerClient(client2);
  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should still be active.");

  yield firstFront.destroy();
  yield closeDebuggerClient(client);
  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should have been automatically stopped.");

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
