/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that integers used in arguments are not cast to their constant, enum value
 * forms if the method's signature does not expect an enum. Bug 999687.
 */

async function ifTestingSupported() {
  const { target, front } = await initCanvasDebuggerBackend(WEBGL_ENUM_URL);

  const navigated = once(target, "navigate");

  await front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  await navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  const snapshotActor = await front.recordAnimationFrame();
  const animationOverview = await snapshotActor.getOverview();
  const functionCalls = animationOverview.calls;

  is(functionCalls[0].name, "clear",
    "The function's name is correct.");
  is(functionCalls[0].argsPreview, "DEPTH_BUFFER_BIT | STENCIL_BUFFER_BIT | COLOR_BUFFER_BIT",
    "The bits passed into `gl.clear` have been cast to their enum values.");

  is(functionCalls[1].name, "bindTexture",
    "The function's name is correct.");
  is(functionCalls[1].argsPreview, "TEXTURE_2D, null",
    "The bits passed into `gl.bindTexture` have been cast to their enum values.");

  await removeTab(target.tab);
  finish();
}
