/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function draw(window, src) {
  const { document, Image } = window;

  const promise = new Promise((resolve, reject) => {
    const img = new Image();

    img.onload = function() {
      // Create a new offscreen canvas
      const canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
      canvas.width = img.naturalWidth;
      canvas.height = img.naturalHeight;
      const ctx = canvas.getContext("2d");

      ctx.drawImage(img, 0, 0);

      resolve(canvas);
    };

    img.onerror = function() {
      reject(`error loading image ${src}`);
    }

    // Load the src image for drawing
    img.src = src;
  });

  return promise;
}

async function compareImages(window, expected, test) {
  const testCanvas = await draw(window, test);
  const expectedCanvas = await draw(window, expected);

  is(testCanvas.width, expectedCanvas.width, "The test and expected images must be the same size");
  is(testCanvas.height, expectedCanvas.height, "The test and expected images must be the same size");

  const nsIDOMWindowUtils = window.getInterface(Ci.nsIDOMWindowUtils);
  return nsIDOMWindowUtils.compareCanvases(expectedCanvas, testCanvas, {});
}

async function cropAndCompare(window, src, expected, test, region) {
  await TestRunner._cropImage(window, src, region, test);

  return compareImages(window, expected, OS.Path.toFileURI(test));
}

add_task(async function crop() {
  const window = Services.wm.getMostRecentWindow("navigator:browser");

  const tmp = OS.Constants.Path.tmpDir;
  is(await cropAndCompare(
      window,
      "chrome://mozscreenshots/content/lib/robot.png",
      "chrome://mozscreenshots/content/lib/robot_upperleft.png",
      OS.Path.join(tmp, "test_cropped_upperleft.png"),
      {x: 0, y: 0, width: 32, height: 32}
  ), 0, "The image should be cropped to the upper left quadrant");

  is(await cropAndCompare(
      window,
      "chrome://mozscreenshots/content/lib/robot.png",
      "chrome://mozscreenshots/content/lib/robot_center.png",
      OS.Path.join(tmp, "test_cropped_center.png"),
      {x: 16, y: 16, width: 32, height: 32}
  ), 0, "The image should be cropped to the center of the image");

  await cropAndCompare(
      window,
      "chrome://mozscreenshots/content/lib/robot.png",
      "chrome://mozscreenshots/content/lib/robot.png",
      OS.Path.join(tmp, "test_cropped_center.png"),
      {x: 16, y: 16, width: 64, height: 64}
  ).then(() => {
    ok(false, "Cropping region should have been too large");
  }, () => {
    ok(true, "Cropping region is too large as expected")
  });
})
