/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if program actors are cached when navigating in the bfcache.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: false });

  reload(target);
  let firstProgram = yield once(front, "program-linked");
  yield checkFirstCachedPrograms(firstProgram);
  yield checkHighlightingInTheFirstPage(firstProgram);
  ok(true, "The cached programs behave correctly before the navigation.");

  navigate(target, MULTIPLE_CONTEXTS_URL);
  let secondProgram = yield once(front, "program-linked");
  let thirdProgram = yield once(front, "program-linked");
  yield checkSecondCachedPrograms(firstProgram, [secondProgram, thirdProgram]);
  yield checkHighlightingInTheSecondPage(secondProgram, thirdProgram);
  ok(true, "The cached programs behave correctly after the navigation.");

  once(front, "program-linked").then(() => {
    ok(false, "Shouldn't have received any more program-linked notifications.");
  });

  yield navigateInHistory(target, "back");
  yield checkFirstCachedPrograms(firstProgram);
  yield checkHighlightingInTheFirstPage(firstProgram);
  ok(true, "The cached programs behave correctly after navigating back.");

  yield navigateInHistory(target, "forward");
  yield checkSecondCachedPrograms(firstProgram, [secondProgram, thirdProgram]);
  yield checkHighlightingInTheSecondPage(secondProgram, thirdProgram);
  ok(true, "The cached programs behave correctly after navigating forward.");

  yield navigateInHistory(target, "back");
  yield checkFirstCachedPrograms(firstProgram);
  yield checkHighlightingInTheFirstPage(firstProgram);
  ok(true, "The cached programs behave correctly after navigating back again.");

  yield navigateInHistory(target, "forward");
  yield checkSecondCachedPrograms(firstProgram, [secondProgram, thirdProgram]);
  yield checkHighlightingInTheSecondPage(secondProgram, thirdProgram);
  ok(true, "The cached programs behave correctly after navigating forward again.");

  yield removeTab(target.tab);
  finish();

  function checkFirstCachedPrograms(programActor) {
    return Task.spawn(function() {
      let programs = yield front.getPrograms();

      is(programs.length, 1,
        "There should be 1 cached program actor.");
      is(programs[0], programActor,
        "The cached program actor was the expected one.");
    })
  }

  function checkSecondCachedPrograms(oldProgramActor, newProgramActors) {
    return Task.spawn(function() {
      let programs = yield front.getPrograms();

      is(programs.length, 2,
        "There should be 2 cached program actors after the navigation.");
      is(programs[0], newProgramActors[0],
        "The first cached program actor was the expected one after the navigation.");
      is(programs[1], newProgramActors[1],
        "The second cached program actor was the expected one after the navigation.");

      isnot(newProgramActors[0], oldProgramActor,
        "The old program actor is not equal to the new first program actor.");
      isnot(newProgramActors[1], oldProgramActor,
        "The old program actor is not equal to the new second program actor.");
    });
  }

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
