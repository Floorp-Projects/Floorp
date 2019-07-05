/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that buffer status is correctly updated in recording models.
 */

"use strict";

var BUFFER_SIZE = 20000;
var config = { bufferSize: BUFFER_SIZE };

add_task(async function() {
  const target = await addTabTarget(MAIN_DOMAIN + "doc_perf.html");

  const front = await target.getFront("performance");

  await front.setProfilerStatusInterval(10);
  const model = await front.startRecording(config);
  const stats = await once(front, "profiler-status");
  is(
    stats.totalSize,
    BUFFER_SIZE,
    `profiler-status event has totalSize: ${stats.totalSize}`
  );
  ok(
    stats.position < BUFFER_SIZE,
    `profiler-status event has position: ${stats.position}`
  );
  ok(
    stats.generation >= 0,
    `profiler-status event has generation: ${stats.generation}`
  );
  ok(stats.isActive, "profiler-status event is isActive");
  is(
    typeof stats.currentTime,
    "number",
    "profiler-status event has currentTime"
  );

  // Halt once more for a buffer status to ensure we're beyond 0
  await once(front, "profiler-status");

  let lastBufferStatus = 0;
  let checkCount = 0;
  while (lastBufferStatus < 1) {
    const currentBufferStatus = front.getBufferUsageForRecording(model);
    ok(
      currentBufferStatus > lastBufferStatus,
      `buffer is more filled than before: ${currentBufferStatus} > ${lastBufferStatus}`
    );
    lastBufferStatus = currentBufferStatus;
    checkCount++;
    await once(front, "profiler-status");
  }

  ok(checkCount >= 1, "atleast 1 event were fired until the buffer was filled");
  is(lastBufferStatus, 1, "buffer usage cannot surpass 100%");
  await front.stopRecording(model);

  is(
    front.getBufferUsageForRecording(model),
    null,
    "buffer usage should be null when no longer recording."
  );

  await target.destroy();
  gBrowser.removeCurrentTab();
});
