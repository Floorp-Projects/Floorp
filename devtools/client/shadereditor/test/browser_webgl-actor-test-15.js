/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if program actors are cached when navigating in the bfcache.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: false });

  // Attach frame scripts if in e10s to perform
  // history navigation via the content
  loadFrameScripts();

  reload(target);
  const firstProgram = await once(front, "program-linked");
  await checkFirstCachedPrograms(firstProgram);
  await checkHighlightingInTheFirstPage(firstProgram);
  ok(true, "The cached programs behave correctly before the navigation.");

  navigate(target, MULTIPLE_CONTEXTS_URL);
  const [secondProgram, thirdProgram] = await getPrograms(front, 2);
  await checkSecondCachedPrograms(firstProgram, [secondProgram, thirdProgram]);
  await checkHighlightingInTheSecondPage(secondProgram, thirdProgram);
  ok(true, "The cached programs behave correctly after the navigation.");

  once(front, "program-linked").then(() => {
    ok(false, "Shouldn't have received any more program-linked notifications.");
  });

  await navigateInHistory(target, "back");
  await checkFirstCachedPrograms(firstProgram);
  await checkHighlightingInTheFirstPage(firstProgram);
  ok(true, "The cached programs behave correctly after navigating back.");

  await navigateInHistory(target, "forward");
  await checkSecondCachedPrograms(firstProgram, [secondProgram, thirdProgram]);
  await checkHighlightingInTheSecondPage(secondProgram, thirdProgram);
  ok(true, "The cached programs behave correctly after navigating forward.");

  await navigateInHistory(target, "back");
  await checkFirstCachedPrograms(firstProgram);
  await checkHighlightingInTheFirstPage(firstProgram);
  ok(true, "The cached programs behave correctly after navigating back again.");

  await navigateInHistory(target, "forward");
  await checkSecondCachedPrograms(firstProgram, [secondProgram, thirdProgram]);
  await checkHighlightingInTheSecondPage(secondProgram, thirdProgram);
  ok(true, "The cached programs behave correctly after navigating forward again.");

  await removeTab(target.tab);
  finish();

  function checkFirstCachedPrograms(programActor) {
    return (async function() {
      const programs = await front.getPrograms();

      is(programs.length, 1,
        "There should be 1 cached program actor.");
      is(programs[0], programActor,
        "The cached program actor was the expected one.");
    })();
  }

  function checkSecondCachedPrograms(oldProgramActor, newProgramActors) {
    return (async function() {
      const programs = await front.getPrograms();

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
    })();
  }

  function checkHighlightingInTheFirstPage(programActor) {
    return (async function() {
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct before highlighting.");

      await programActor.highlight([0, 1, 0, 1]);
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
      await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct after highlighting.");

      await programActor.unhighlight();
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct after unhighlighting.");
    })();
  }

  function checkHighlightingInTheSecondPage(firstProgramActor, secondProgramActor) {
    return (async function() {
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The two canvases are correctly drawn before highlighting.");

      await firstProgramActor.highlight([1, 0, 0, 1]);
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The first canvas was correctly filled after highlighting.");

      await secondProgramActor.highlight([0, 1, 0, 1]);
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 0, a: 255 }, true, "#canvas2");
      await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
      await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 0, a: 255 }, true, "#canvas2");
      ok(true, "The second canvas was correctly filled after highlighting.");

      await firstProgramActor.unhighlight();
      await secondProgramActor.unhighlight();
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
      await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
      ok(true, "The two canvases were correctly filled after unhighlighting.");
    })();
  }
}
