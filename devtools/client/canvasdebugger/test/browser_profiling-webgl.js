/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if functions inside a single animation frame are recorded and stored
 * for a canvas context profiling.
 */

function* ifTestingSupported() {
  let currentTime = window.performance.now();
  info("Start to estimate WebGL drawArrays function.");
  var { target, front } = yield initCanvasDebuggerBackend(WEBGL_DRAW_ARRAYS);

  let navigated = once(target, "navigate");

  yield front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  yield navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  let snapshotActor = yield front.recordAnimationFrame();
  ok(snapshotActor,
    "A snapshot actor was sent after recording.");

  let animationOverview = yield snapshotActor.getOverview();
  ok(animationOverview,
    "An animation overview could be retrieved after recording.");

  let functionCalls = animationOverview.calls;
  ok(functionCalls,
    "An array of function call actors was sent after recording.");

  testFunctionCallTimestamp(functionCalls, currentTime);

  info("Check triangle and vertex counts in drawArrays()");
  is(animationOverview.primitive.tris, 5, "The count of triangles is correct.");
  is(animationOverview.primitive.vertices, 26, "The count of vertices is correct.");
  is(animationOverview.primitive.points, 4, "The count of points is correct.");
  is(animationOverview.primitive.lines, 8, "The count of lines is correct.");

  yield removeTab(target.tab);

  info("Start to estimate WebGL drawElements function.");
  var { target, front } = yield initCanvasDebuggerBackend(WEBGL_DRAW_ELEMENTS);

  navigated = once(target, "navigate");

  yield front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  yield navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  snapshotActor = yield front.recordAnimationFrame();
  ok(snapshotActor,
    "A snapshot actor was sent after recording.");
  
  animationOverview = yield snapshotActor.getOverview();
  ok(animationOverview,
    "An animation overview could be retrieved after recording.");
  
  functionCalls = animationOverview.calls;
  ok(functionCalls,
    "An array of function call actors was sent after recording.");
  
  testFunctionCallTimestamp(functionCalls, currentTime);
  
  info("Check triangle and vertex counts in drawElements()");
  is(animationOverview.primitive.tris, 5, "The count of triangles is correct.");
  is(animationOverview.primitive.vertices, 26, "The count of vertices is correct.");
  is(animationOverview.primitive.points, 4, "The count of points is correct.");
  is(animationOverview.primitive.lines, 8, "The count of lines is correct.");
  
  yield removeTab(target.tab);
  finish();
}

function testFunctionCallTimestamp(functionCalls, currentTime) {

  info("Check the timestamps of function calls");

  for ( let i = 0; i < functionCalls.length-1; i += 2 ) {
    ok( functionCalls[i].timestamp > 0, "The timestamp of the called function is larger than 0." );
    ok( functionCalls[i].timestamp < currentTime, "The timestamp has been minus the frame start time." );
    ok( functionCalls[i+1].timestamp > functionCalls[i].timestamp, "The timestamp of the called function is correct." );
  }

  yield removeTab(target.tab);
  finish();
}
