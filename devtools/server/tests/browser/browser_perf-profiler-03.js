/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the built-in profiler module is not reactivated if no other
 * consumer was using it over the remote debugger protocol, and ensures
 * that the actor will work properly even in such cases (e.g. the Gecko Profiler
 * addon was installed and automatically activated the profiler module).
 */

"use strict";

const {
  pmmIsProfilerActive,
  pmmStartProfiler,
  pmmLoadFrameScripts,
  pmmClearFrameScripts,
} = require("devtools/client/performance/test/helpers/profiler-mm-utils");

add_task(async function() {
  // Ensure the profiler is already running when the test starts.
  pmmLoadFrameScripts(gBrowser);
  const entries = 1000000;
  const interval = 1;
  const features = ["js"];
  await pmmStartProfiler({ entries, interval, features });

  ok(
    await pmmIsProfilerActive(),
    "The built-in profiler module should still be active."
  );

  const target1 = await addTabTarget(MAIN_DOMAIN + "doc_perf.html");
  const firstFront = await target1.getFront("performance");

  await firstFront.startRecording();

  const target2 = await addTabTarget(MAIN_DOMAIN + "doc_perf.html");
  const secondFront = await target2.getFront("performance");
  await secondFront.connect();

  await target2.destroy();
  ok(
    await pmmIsProfilerActive(),
    "The built-in profiler module should still be active."
  );

  await target1.destroy();
  ok(
    !(await pmmIsProfilerActive()),
    "The built-in profiler module should have been automatically stopped."
  );

  pmmClearFrameScripts();

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
