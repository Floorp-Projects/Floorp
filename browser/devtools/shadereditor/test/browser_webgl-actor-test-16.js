/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if program actors are invalidated from the cache when a window is
 * removed from the bfcache.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: false });

  // 0. Perform the initial reload.

  reload(target);
  let firstProgram = yield once(front, "program-linked");
  let programs = yield front.getPrograms();
  is(programs.length, 1,
    "The first program should be returned by a call to getPrograms().");
  is(programs[0], firstProgram,
    "The first programs was correctly retrieved from the cache.");

  // 1. Perform a simple navigation.

  navigate(target, MULTIPLE_CONTEXTS_URL);
  let secondProgram = yield once(front, "program-linked");
  let thirdProgram = yield once(front, "program-linked");
  let programs = yield front.getPrograms();
  is(programs.length, 2,
    "The second and third programs should be returned by a call to getPrograms().");
  is(programs[0], secondProgram,
    "The second programs was correctly retrieved from the cache.");
  is(programs[1], thirdProgram,
    "The third programs was correctly retrieved from the cache.");

  // 2. Perform a bfcache navigation.

  yield navigateInHistory(target, "back");
  let globalDestroyed = observe("inner-window-destroyed");
  let globalCreated = observe("content-document-global-created");
  reload(target);

  yield globalDestroyed;
  let programs = yield front.getPrograms();
  is(programs.length, 0,
    "There should be no cached program actors yet.");

  yield globalCreated;
  let programs = yield front.getPrograms();
  is(programs.length, 1,
    "There should be 1 cached program actor now.");

  yield checkHighlightingInTheFirstPage(programs[0]);
  ok(true, "The cached programs behave correctly after navigating back and reloading.");

  // 3. Perform a bfcache navigation and a page reload.

  yield navigateInHistory(target, "forward");
  let globalDestroyed = observe("inner-window-destroyed");
  let globalCreated = observe("content-document-global-created");
  reload(target);

  yield globalDestroyed;
  let programs = yield front.getPrograms();
  is(programs.length, 0,
    "There should be no cached program actors yet.");

  yield once(front, "program-linked");
  yield once(front, "program-linked");
  yield globalCreated;
  let programs = yield front.getPrograms();
  is(programs.length, 2,
    "There should be 2 cached program actors now.");

  yield checkHighlightingInTheSecondPage(programs[0], programs[1]);
  ok(true, "The cached programs behave correctly after navigating forward and reloading.");

  yield removeTab(target.tab);
  finish();

  function checkHighlightingInTheFirstPage(programActor) {
    return Task.spawn(function() {
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      ok(true, "The top left pixel color was correct before highlighting.");

      yield programActor.highlight([0, 0, 1, 1]);
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 0, b: 255, a: 255 }, true);
      ok(true, "The top left pixel color is correct after highlighting.");

      yield programActor.unhighlight();
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      ok(true, "The top left pixel color is correct after unhighlighting.");
    });
  }

  function checkHighlightingInTheSecondPage(firstProgramActor, secondProgramActor) {
    return Task.spawn(function() {
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The two canvases are correctly drawn before highlighting.");

      yield firstProgramActor.highlight([1, 0, 0, 1]);
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The first canvas was correctly filled after highlighting.");

      yield secondProgramActor.highlight([0, 1, 0, 1]);
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 255, b: 0, a: 255 }, true, "#canvas2");
      yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 255, b: 0, a: 255 }, true, "#canvas2");
      ok(true, "The second canvas was correctly filled after highlighting.");

      yield firstProgramActor.unhighlight();
      yield secondProgramActor.unhighlight();
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The two canvases were correctly filled after unhighlighting.");
    });
  }
}
