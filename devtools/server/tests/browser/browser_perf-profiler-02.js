/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the built-in profiler module doesn't deactivate when the toolbox
 * is destroyed if there are other consumers using it.
 */

"use strict";

const {
  pmmIsProfilerActive,
  pmmLoadFrameScripts,
} = require("devtools/client/performance/test/helpers/profiler-mm-utils");

add_task(async function() {
  const target1 = await addTabTarget(MAIN_DOMAIN + "doc_perf.html");
  const firstFront = await target1.getFront("performance");

  pmmLoadFrameScripts(gBrowser);

  await firstFront.startRecording();

  const target2 = await addTabTarget(MAIN_DOMAIN + "doc_perf.html");
  const secondFront = await target2.getFront("performance");
  await secondFront.connect();
  pmmLoadFrameScripts(gBrowser);

  await secondFront.startRecording();

  // Manually teardown the tabs so we can check profiler status
  await target2.destroy();
  ok(
    await pmmIsProfilerActive(),
    "The built-in profiler module should still be active."
  );

  await target1.destroy();
  ok(
    !(await pmmIsProfilerActive()),
    "The built-in profiler module should no longer be active."
  );

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
