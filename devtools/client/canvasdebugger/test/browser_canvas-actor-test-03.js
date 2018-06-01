/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if functions inside a single animation frame are recorded and stored
 * for a canvas context.
 */

async function ifTestingSupported() {
  const { target, front } = await initCanvasDebuggerBackend(SIMPLE_CANVAS_URL);

  const navigated = once(target, "navigate");

  await front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  await navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  const snapshotActor = await front.recordAnimationFrame();
  ok(snapshotActor,
    "A snapshot actor was sent after recording.");

  const animationOverview = await snapshotActor.getOverview();
  ok(snapshotActor,
    "An animation overview could be retrieved after recording.");

  const functionCalls = animationOverview.calls;
  ok(functionCalls,
    "An array of function call actors was sent after recording.");
  is(functionCalls.length, 8,
    "The number of function call actors is correct.");

  is(functionCalls[0].type, CallWatcherFront.METHOD_FUNCTION,
    "The first called function is correctly identified as a method.");
  is(functionCalls[0].name, "clearRect",
    "The first called function's name is correct.");
  is(functionCalls[0].file, SIMPLE_CANVAS_URL,
    "The first called function's file is correct.");
  is(functionCalls[0].line, 25,
    "The first called function's line is correct.");
  is(functionCalls[0].argsPreview, "0, 0, 128, 128",
    "The first called function's args preview is correct.");
  is(functionCalls[0].callerPreview, "Object",
    "The first called function's caller preview is correct.");

  is(functionCalls[6].type, CallWatcherFront.METHOD_FUNCTION,
    "The penultimate called function is correctly identified as a method.");
  is(functionCalls[6].name, "fillRect",
    "The penultimate called function's name is correct.");
  is(functionCalls[6].file, SIMPLE_CANVAS_URL,
    "The penultimate called function's file is correct.");
  is(functionCalls[6].line, 21,
    "The penultimate called function's line is correct.");
  is(functionCalls[6].argsPreview, "10, 10, 55, 50",
    "The penultimate called function's args preview is correct.");
  is(functionCalls[6].callerPreview, "Object",
    "The penultimate called function's caller preview is correct.");

  is(functionCalls[7].type, CallWatcherFront.METHOD_FUNCTION,
    "The last called function is correctly identified as a method.");
  is(functionCalls[7].name, "requestAnimationFrame",
    "The last called function's name is correct.");
  is(functionCalls[7].file, SIMPLE_CANVAS_URL,
    "The last called function's file is correct.");
  is(functionCalls[7].line, 30,
    "The last called function's line is correct.");
  ok(functionCalls[7].argsPreview.includes("Function"),
    "The last called function's args preview is correct.");
  is(functionCalls[7].callerPreview, "Object",
    "The last called function's caller preview is correct.");

  await removeTab(target.tab);
  finish();
}
