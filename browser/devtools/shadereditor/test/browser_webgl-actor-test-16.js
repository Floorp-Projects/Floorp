/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if program actors are invalidated from the cache when a window is
 * removed from the bfcache.
 */

function* ifWebGLSupported() {
  let { target, front } = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: false });

  // Attach frame scripts if in e10s to perform
  // history navigation via the content
  loadFrameScripts();

  // 0. Perform the initial reload.

  reload(target);
  let firstProgram = yield once(front, "program-linked");
  let programs = yield front.getPrograms();
  is(programs.length, 1,
    "The first program should be returned by a call to getPrograms().");
  is(programs[0], firstProgram,
    "The first programs was correctly retrieved from the cache.");

  let allPrograms = yield front._getAllPrograms();
  is(allPrograms.length, 1,
    "Should be only one program in cache.");

  // 1. Perform a simple navigation.

  navigate(target, MULTIPLE_CONTEXTS_URL);
  let [secondProgram, thirdProgram] = yield getPrograms(front, 2);
  programs = yield front.getPrograms();
  is(programs.length, 2,
    "The second and third programs should be returned by a call to getPrograms().");
  is(programs[0], secondProgram,
    "The second programs was correctly retrieved from the cache.");
  is(programs[1], thirdProgram,
    "The third programs was correctly retrieved from the cache.");

  allPrograms = yield front._getAllPrograms();
  is(allPrograms.length, 3,
    "Should be three programs in cache.");

  // 2. Perform a bfcache navigation.

  yield navigateInHistory(target, "back");
  let globalDestroyed = once(front, "global-created");
  let globalCreated = once(front, "global-destroyed");
  let programsLinked = once(front, "program-linked");
  reload(target);

  yield promise.all([programsLinked, globalDestroyed, globalCreated]);
  allPrograms = yield front._getAllPrograms();
  is(allPrograms.length, 3,
    "Should be 3 programs total in cache.");

  programs = yield front.getPrograms();
  is(programs.length, 1,
    "There should be 1 cached program actor now.");

  yield checkHighlightingInTheFirstPage(programs[0]);
  ok(true, "The cached programs behave correctly after navigating back and reloading.");

  // 3. Perform a bfcache navigation and a page reload.

  yield navigateInHistory(target, "forward");

  globalDestroyed = once(front, "global-created");
  globalCreated = once(front, "global-destroyed");
  programsLinked = getPrograms(front, 2);

  reload(target);

  yield promise.all([programsLinked, globalDestroyed, globalCreated]);
  allPrograms = yield front._getAllPrograms();
  is(allPrograms.length, 3,
    "Should be 3 programs total in cache.");

  programs = yield front.getPrograms();
  is(programs.length, 2,
    "There should be 2 cached program actors now.");

  yield checkHighlightingInTheSecondPage(programs[0], programs[1]);
  ok(true, "The cached programs behave correctly after navigating forward and reloading.");

  yield removeTab(target.tab);
  finish();

  function checkHighlightingInTheFirstPage(programActor) {
    return Task.spawn(function*() {
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct before highlighting.");

      yield programActor.highlight([0, 1, 0, 1]);
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
      yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct after highlighting.");

      yield programActor.unhighlight();
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct after unhighlighting.");
    });
  }

  function checkHighlightingInTheSecondPage(firstProgramActor, secondProgramActor) {
    return Task.spawn(function*() {
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The two canvases are correctly drawn before highlighting.");

      yield firstProgramActor.highlight([1, 0, 0, 1]);
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The first canvas was correctly filled after highlighting.");

      yield secondProgramActor.highlight([0, 1, 0, 1]);
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 0, a: 255 }, true, "#canvas2");
      yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 0, a: 255 }, true, "#canvas2");
      ok(true, "The second canvas was correctly filled after highlighting.");

      yield firstProgramActor.unhighlight();
      yield secondProgramActor.unhighlight();
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The two canvases were correctly filled after unhighlighting.");
    });
  }
}
