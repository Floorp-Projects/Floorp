/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the correct framebuffer, renderbuffer and textures are re-bound
 * after generating screenshots using the actor.
 */

function* ifTestingSupported() {
  let { target, front } = yield initCanvasDebuggerBackend(WEBGL_BINDINGS_URL);
  loadFrameScripts();

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
  is(firstScreenshot.scaling, 0.25,
    "The first screenshot has the correct scaling.");
  is(firstScreenshot.width, CanvasFront.WEBGL_SCREENSHOT_MAX_HEIGHT,
    "The first screenshot has the correct width.");
  is(firstScreenshot.height, CanvasFront.WEBGL_SCREENSHOT_MAX_HEIGHT,
    "The first screenshot has the correct height.");
  is(firstScreenshot.flipped, true,
    "The first screenshot has the correct 'flipped' flag.");
  is(firstScreenshot.pixels.length, 0,
    "The first screenshot should be empty.");

  is((yield evalInDebuggee("gl.getParameter(gl.FRAMEBUFFER_BINDING) === customFramebuffer")),
    true,
    "The debuggee's gl context framebuffer wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.RENDERBUFFER_BINDING) === customRenderbuffer")),
    true,
    "The debuggee's gl context renderbuffer wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.TEXTURE_BINDING_2D) === customTexture")),
    true,
    "The debuggee's gl context texture binding wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.VIEWPORT)[0]")),
    128,
    "The debuggee's gl context viewport's left coord. wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.VIEWPORT)[1]")),
    256,
    "The debuggee's gl context viewport's left coord. wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.VIEWPORT)[2]")),
    384,
    "The debuggee's gl context viewport's left coord. wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.VIEWPORT)[3]")),
    512,
    "The debuggee's gl context viewport's left coord. wasn't changed.");

  let secondScreenshot = yield snapshotActor.generateScreenshotFor(functionCalls[1]);
  is(secondScreenshot.index, 1,
    "The second screenshot has the correct index.");
  is(secondScreenshot.width, CanvasFront.WEBGL_SCREENSHOT_MAX_HEIGHT,
    "The second screenshot has the correct width.");
  is(secondScreenshot.height, CanvasFront.WEBGL_SCREENSHOT_MAX_HEIGHT,
    "The second screenshot has the correct height.");
  is(secondScreenshot.scaling, 0.25,
    "The second screenshot has the correct scaling.");
  is(secondScreenshot.flipped, true,
    "The second screenshot has the correct 'flipped' flag.");
  is(secondScreenshot.pixels.length, Math.pow(CanvasFront.WEBGL_SCREENSHOT_MAX_HEIGHT, 2) * 4,
    "The second screenshot should not be empty.");
  is(secondScreenshot.pixels[0], 0,
    "The second screenshot has the correct red component.");
  is(secondScreenshot.pixels[1], 0,
    "The second screenshot has the correct green component.");
  is(secondScreenshot.pixels[2], 255,
    "The second screenshot has the correct blue component.");
  is(secondScreenshot.pixels[3], 255,
    "The second screenshot has the correct alpha component.");

  is((yield evalInDebuggee("gl.getParameter(gl.FRAMEBUFFER_BINDING) === customFramebuffer")),
    true,
    "The debuggee's gl context framebuffer still wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.RENDERBUFFER_BINDING) === customRenderbuffer")),
    true,
    "The debuggee's gl context renderbuffer still wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.TEXTURE_BINDING_2D) === customTexture")),
    true,
    "The debuggee's gl context texture binding still wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.VIEWPORT)[0]")),
    128,
    "The debuggee's gl context viewport's left coord. still wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.VIEWPORT)[1]")),
    256,
    "The debuggee's gl context viewport's left coord. still wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.VIEWPORT)[2]")),
    384,
    "The debuggee's gl context viewport's left coord. still wasn't changed.");
  is((yield evalInDebuggee("gl.getParameter(gl.VIEWPORT)[3]")),
    512,
    "The debuggee's gl context viewport's left coord. still wasn't changed.");

  yield removeTab(target.tab);
  finish();
}
