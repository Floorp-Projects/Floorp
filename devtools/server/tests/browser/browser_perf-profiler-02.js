/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the built-in profiler module doesn't deactivate when the toolbox
 * is destroyed if there are other consumers using it.
 */

const { PerformanceFront } = require("devtools/server/actors/performance");
const { sendProfilerCommand, PMM_isProfilerActive, PMM_stopProfiler, PMM_loadFrameScripts } = require("devtools/shared/performance/process-communication");

add_task(function*() {
  yield addTab(MAIN_DOMAIN + "doc_perf.html");
  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let firstFront = PerformanceFront(client, form);
  yield firstFront.connect();

  PMM_loadFrameScripts(gBrowser);

  yield firstFront.startRecording();

  yield addTab(MAIN_DOMAIN + "doc_perf.html");
  let client2 = new DebuggerClient(DebuggerServer.connectPipe());
  let form2 = yield connectDebuggerClient(client2);
  let secondFront = PerformanceFront(client2, form2);
  yield secondFront.connect();
  PMM_loadFrameScripts(gBrowser);

  yield secondFront.startRecording();

  // Manually teardown the tabs so we can check profiler status
  yield secondFront.destroy();
  yield closeDebuggerClient(client2);
  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should still be active.");

  yield firstFront.destroy();
  yield closeDebuggerClient(client);
  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should no longer be active.");

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
