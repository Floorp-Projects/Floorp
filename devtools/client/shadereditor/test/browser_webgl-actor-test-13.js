/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if multiple WebGL contexts are correctly handled.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(MULTIPLE_CONTEXTS_URL);
  front.setup({ reload: true });

  const [firstProgramActor, secondProgramActor] = await getPrograms(front, 2);

  isnot(firstProgramActor, secondProgramActor,
    "Two distinct program actors were recevide from two separate contexts.");

  const firstVertexShader = await firstProgramActor.getVertexShader();
  const firstFragmentShader = await firstProgramActor.getFragmentShader();
  const secondVertexShader = await secondProgramActor.getVertexShader();
  const secondFragmentShader = await secondProgramActor.getFragmentShader();

  isnot(firstVertexShader, secondVertexShader,
    "The two programs should have distinct vertex shaders.");
  isnot(firstFragmentShader, secondFragmentShader,
    "The two programs should have distinct fragment shaders.");

  const firstVertSource = await firstVertexShader.getText();
  const firstFragSource = await firstFragmentShader.getText();
  const secondVertSource = await secondVertexShader.getText();
  const secondFragSource = await secondFragmentShader.getText();

  is(firstVertSource, secondVertSource,
    "The vertex shaders should have identical sources.");
  is(firstFragSource, secondFragSource,
    "The vertex shaders should have identical sources.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The two canvases are correctly drawn.");

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

  await removeTab(target.tab);
  finish();
}
