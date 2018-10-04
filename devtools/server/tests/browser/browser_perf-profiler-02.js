/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the built-in profiler module doesn't deactivate when the toolbox
 * is destroyed if there are other consumers using it.
 */

"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");
const { pmmIsProfilerActive, pmmLoadFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");

add_task(async function() {
  await addTab(MAIN_DOMAIN + "doc_perf.html");
  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const firstFront = PerformanceFront(client, form);
  await firstFront.connect();

  pmmLoadFrameScripts(gBrowser);

  await firstFront.startRecording();

  await addTab(MAIN_DOMAIN + "doc_perf.html");
  const client2 = new DebuggerClient(DebuggerServer.connectPipe());
  const form2 = await connectDebuggerClient(client2);
  const secondFront = PerformanceFront(client2, form2);
  await secondFront.connect();
  pmmLoadFrameScripts(gBrowser);

  await secondFront.startRecording();

  // Manually teardown the tabs so we can check profiler status
  await secondFront.destroy();
  await client2.close();
  ok((await pmmIsProfilerActive()),
    "The built-in profiler module should still be active.");

  await firstFront.destroy();
  await client.close();
  ok(!(await pmmIsProfilerActive()),
    "The built-in profiler module should no longer be active.");

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
