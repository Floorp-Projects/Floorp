/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if functions calls are recorded and stored for a canvas context,
 * and that their stack is successfully retrieved.
 */

async function ifTestingSupported() {
  const { target, front } = await initCallWatcherBackend(SIMPLE_CANVAS_URL);

  const navigated = once(target, "navigate");

  await front.setup({
    tracedGlobals: ["CanvasRenderingContext2D", "WebGLRenderingContext"],
    startRecording: true,
    performReload: true,
    storeCalls: true
  });
  ok(true, "The front was setup up successfully.");

  await navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  // Allow the content to execute some functions.
  await waitForTick();

  const functionCalls = await front.pauseRecording();
  ok(functionCalls,
    "An array of function call actors was sent after reloading.");
  ok(functionCalls.length > 0,
    "There's at least one function call actor available.");

  is(functionCalls[0].type, CallWatcherFront.METHOD_FUNCTION,
    "The called function is correctly identified as a method.");
  is(functionCalls[0].name, "clearRect",
    "The called function's name is correct.");
  is(functionCalls[0].file, SIMPLE_CANVAS_URL,
    "The called function's file is correct.");
  is(functionCalls[0].line, 25,
    "The called function's line is correct.");

  is(functionCalls[0].callerPreview, "Object",
    "The called function's caller preview is correct.");
  is(functionCalls[0].argsPreview, "0, 0, 128, 128",
    "The called function's args preview is correct.");

  const details = await functionCalls[1].getDetails();
  ok(details,
    "The first called function has some details available.");

  is(details.stack.length, 3,
    "The called function's stack depth is correct.");

  is(details.stack[0].name, "fillStyle",
    "The called function's stack is correct (1.1).");
  is(details.stack[0].file, SIMPLE_CANVAS_URL,
    "The called function's stack is correct (1.2).");
  is(details.stack[0].line, 20,
    "The called function's stack is correct (1.3).");

  is(details.stack[1].name, "drawRect",
    "The called function's stack is correct (2.1).");
  is(details.stack[1].file, SIMPLE_CANVAS_URL,
    "The called function's stack is correct (2.2).");
  is(details.stack[1].line, 26,
    "The called function's stack is correct (2.3).");

  is(details.stack[2].name, "drawScene",
    "The called function's stack is correct (3.1).");
  is(details.stack[2].file, SIMPLE_CANVAS_URL,
    "The called function's stack is correct (3.2).");
  is(details.stack[2].line, 33,
    "The called function's stack is correct (3.3).");

  await removeTab(target.tab);
  finish();
}
