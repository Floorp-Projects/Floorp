/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the correct framebuffer, renderbuffer and textures are re-bound
 * after generating screenshots using the actor.
 */

function ifTestingSupported() {
  let [target, debuggee, front] = yield initCanavsDebuggerBackend(WEBGL_BINDINGS_URL);

  let navigated = once(target, "navigate");

  yield front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  yield navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  let snapshotActor = yield front.recordAnimationFrame();
  let animationOverview = yield snapshotActor.getOverview();
  let functionCalls = animationOverview.calls;

  let firstScreenshot = yield snapshotActor.generateScreenshotFor(functionCalls[0]);
  is(firstScreenshot.index, -1,
    "The first screenshot didn't encounter any draw call.");
  is(firstScreenshot.width, 128,
    "The first screenshot has the correct width.");
  is(firstScreenshot.height, 128,
    "The first screenshot has the correct height.");
  is(firstScreenshot.flipped, true,
    "The first screenshot has the correct 'flipped' flag.");
  is(firstScreenshot.pixels.length, 0,
    "The first screenshot should be empty.");

  let gl = debuggee.gl;
  is(gl.getParameter(gl.FRAMEBUFFER_BINDING), debuggee.customFramebuffer,
    "The debuggee's gl context framebuffer wasn't changed.");
  is(gl.getParameter(gl.RENDERBUFFER_BINDING), debuggee.customRenderbuffer,
    "The debuggee's gl context renderbuffer wasn't changed.");
  is(gl.getParameter(gl.TEXTURE_BINDING_2D), debuggee.customTexture,
    "The debuggee's gl context texture binding wasn't changed.");

  let secondScreenshot = yield snapshotActor.generateScreenshotFor(functionCalls[1]);
  is(secondScreenshot.index, 1,
    "The second screenshot has the correct index.");
  is(secondScreenshot.width, 128,
    "The second screenshot has the correct width.");
  is(secondScreenshot.height, 128,
    "The second screenshot has the correct height.");
  is(secondScreenshot.flipped, true,
    "The second screenshot has the correct 'flipped' flag.");
  is(secondScreenshot.pixels.length, 128 * 128,
    "The second screenshot should not be empty.");
  is(new Uint8Array(secondScreenshot.pixels.buffer)[0], 0,
    "The second screenshot has the correct red component.");
  is(new Uint8Array(secondScreenshot.pixels.buffer)[1], 0,
    "The second screenshot has the correct green component.");
  is(new Uint8Array(secondScreenshot.pixels.buffer)[2], 255,
    "The second screenshot has the correct blue component.");
  is(new Uint8Array(secondScreenshot.pixels.buffer)[3], 255,
    "The second screenshot has the correct alpha component.");

  let gl = debuggee.gl;
  is(gl.getParameter(gl.FRAMEBUFFER_BINDING), debuggee.customFramebuffer,
    "The debuggee's gl context framebuffer still wasn't changed.");
  is(gl.getParameter(gl.RENDERBUFFER_BINDING), debuggee.customRenderbuffer,
    "The debuggee's gl context renderbuffer still wasn't changed.");
  is(gl.getParameter(gl.TEXTURE_BINDING_2D), debuggee.customTexture,
    "The debuggee's gl context texture binding still wasn't changed.");

  yield removeTab(target.tab);
  finish();
}
