/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler connection front does not activate the built-in
 * profiler module if not necessary, and doesn't deactivate it when
 * a recording is stopped.
 */

"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");
const { pmmIsProfilerActive, pmmLoadFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");

add_task(async function() {
  await addTab(MAIN_DOMAIN + "doc_perf.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = PerformanceFront(client, form);
  await front.connect();

  pmmLoadFrameScripts(gBrowser);

  ok(!(await pmmIsProfilerActive()),
    "The built-in profiler module should not have been automatically started.");

  let rec = await front.startRecording();
  await front.stopRecording(rec);
  ok((await pmmIsProfilerActive()),
    "The built-in profiler module should still be active (1).");

  rec = await front.startRecording();
  await front.stopRecording(rec);
  ok((await pmmIsProfilerActive()),
    "The built-in profiler module should still be active (2).");

  await front.destroy();
  await client.close();

  ok(!(await pmmIsProfilerActive()),
    "The built-in profiler module should no longer be active.");

  gBrowser.removeCurrentTab();
});
