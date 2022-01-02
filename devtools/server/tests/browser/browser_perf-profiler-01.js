/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler connection front does not activate the built-in
 * profiler module if not necessary, and doesn't deactivate it when
 * a recording is stopped.
 */

"use strict";

const {
  pmmIsProfilerActive,
  pmmInitWithBrowser,
} = require("devtools/client/performance/test/helpers/profiler-mm-utils");

add_task(async function() {
  const target = await addTabTarget(MAIN_DOMAIN + "doc_perf.html");

  const front = await target.getFront("performance");

  pmmInitWithBrowser(gBrowser);

  ok(
    !(await pmmIsProfilerActive()),
    "The built-in profiler module should not have been automatically started."
  );

  let rec = await front.startRecording();
  await front.stopRecording(rec);
  ok(
    await pmmIsProfilerActive(),
    "The built-in profiler module should still be active (1)."
  );

  rec = await front.startRecording();
  await front.stopRecording(rec);
  ok(
    await pmmIsProfilerActive(),
    "The built-in profiler module should still be active (2)."
  );

  await target.destroy();

  ok(
    !(await pmmIsProfilerActive()),
    "The built-in profiler module should no longer be active."
  );

  gBrowser.removeCurrentTab();
});
