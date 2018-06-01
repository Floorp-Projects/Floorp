/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that integers used in arguments are not cast to their constant, enum value
 * forms if the method's signature does not expect an enum. Bug 999687.
 */

async function ifTestingSupported() {
  const { target, front } = await initCanvasDebuggerBackend(SIMPLE_BITMASKS_URL);

  const navigated = once(target, "navigate");

  await front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  await navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  const snapshotActor = await front.recordAnimationFrame();
  const animationOverview = await snapshotActor.getOverview();
  const functionCalls = animationOverview.calls;

  is(functionCalls[0].name, "clearRect",
    "The first called function's name is correct.");
  is(functionCalls[0].argsPreview, "0, 0, 4, 4",
    "The first called function's args preview is not cast to enums.");

  is(functionCalls[2].name, "fillRect",
    "The fillRect called function's name is correct.");
  is(functionCalls[2].argsPreview, "0, 0, 1, 1",
    "The fillRect called function's args preview is not casted to enums.");

  await removeTab(target.tab);
  finish();
}
