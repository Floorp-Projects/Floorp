/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that buffer status is correctly updated in recording models.
 */

let BUFFER_SIZE = 20000;

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL, { TEST_MOCK_BUFFER_CHECK_TIMER: 10 });
  let config = { bufferSize: BUFFER_SIZE };

  let model = yield front.startRecording(config);
  let [_, stats] = yield onceSpread(front, "buffer-status");
  is(stats.totalSize, BUFFER_SIZE, `buffer-status event has correct totalSize: ${stats.totalSize}`);
  ok(stats.position < BUFFER_SIZE, `buffer-status event has correct position: ${stats.position}`);
  is(stats.generation, 0, `buffer-status event has correct generation: ${stats.generation}`);

  // Halt once more for a buffer status to ensure we're beyond 0
  yield once(front, "buffer-status");

  let lastBufferStatus = 0;
  let checkCount = 0;
  while (lastBufferStatus < 1) {
    let currentBufferStatus = model.getBufferUsage();
    ok(currentBufferStatus > lastBufferStatus, `buffer is more filled than before: ${currentBufferStatus}`);
    lastBufferStatus = currentBufferStatus;
    checkCount++;
    yield once(front, "buffer-status");
  }

  ok(checkCount >= 1, "atleast 1 event were fired until the buffer was filled");
  is(lastBufferStatus, 1, "buffer usage cannot surpass 100%");
  yield front.stopRecording(model);

  is(model.getBufferUsage(), null, "getBufferUsage() should be null when no longer recording.");

  yield removeTab(target.tab);
  finish();
}
