/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the recording can be disabled via stopRecordingAnimationFrame
 * in the event no rAF loop is found.
 */

async function ifTestingSupported() {
  const { target, front } = await initCanvasDebuggerBackend(NO_CANVAS_URL);
  loadFrameScriptUtils();

  const navigated = once(target, "navigate");

  await front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  await navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  const startRecording = front.recordAnimationFrame();
  await front.stopRecordingAnimationFrame();

  ok(!(await startRecording),
    "recordAnimationFrame() does not return a SnapshotActor when cancelled.");

  await removeTab(target.tab);
  finish();
}
