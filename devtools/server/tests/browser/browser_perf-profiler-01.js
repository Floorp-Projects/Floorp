/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler connection front does not activate the built-in
 * profiler module if not necessary, and doesn't deactivate it when
 * a recording is stopped.
 */

const { PerformanceFront } = require("devtools/shared/fronts/performance");
const { pmmIsProfilerActive, pmmStopProfiler, pmmLoadFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");

add_task(function* () {
  let browser = yield addTab(MAIN_DOMAIN + "doc_perf.html");
  let doc = browser.contentDocument;

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();

  pmmLoadFrameScripts(gBrowser);

  ok(!(yield pmmIsProfilerActive()),
    "The built-in profiler module should not have been automatically started.");

  let rec = yield front.startRecording();
  yield front.stopRecording(rec);
  ok((yield pmmIsProfilerActive()),
    "The built-in profiler module should still be active (1).");

  rec = yield front.startRecording();
  yield front.stopRecording(rec);
  ok((yield pmmIsProfilerActive()),
    "The built-in profiler module should still be active (2).");

  yield front.destroy();
  yield client.close();

  ok(!(yield pmmIsProfilerActive()),
    "The built-in profiler module should no longer be active.");

  gBrowser.removeCurrentTab();
});
