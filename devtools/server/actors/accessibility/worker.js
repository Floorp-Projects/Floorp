/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Import `createTask` to communicate with `devtools/shared/worker`.
 */
importScripts("resource://gre/modules/workers/require.js");
const { createTask } = require("resource://devtools/shared/worker/helper.js");

/**
 * @see LineGraphWidget.prototype.setDataFromTimestamps in Graphs.js
 * @param number id
 * @param array timestamps
 * @param number interval
 * @param number duration
 */
createTask(self, "getBgRGBA", ({ dataTextBuf, dataBackgroundBuf }) =>
  getBgRGBA(dataTextBuf, dataBackgroundBuf)
);

/**
 * Calculates the luminance of a rgba tuple based on the formula given in
 * https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
 *
 * @param {Array} rgba An array with [r,g,b,a] values.
 * @return {Number} The calculated luminance.
 */
function calculateLuminance(rgba) {
  for (let i = 0; i < 3; i++) {
    rgba[i] /= 255;
    rgba[i] =
      rgba[i] < 0.03928
        ? rgba[i] / 12.92
        : Math.pow((rgba[i] + 0.055) / 1.055, 2.4);
  }
  return 0.2126 * rgba[0] + 0.7152 * rgba[1] + 0.0722 * rgba[2];
}

/**
 * Get RGBA or a range of RGBAs for the background pixels under the text. If luminance is
 * uniform, only return one value of RGBA, otherwise return values that correspond to the
 * min and max luminances.
 * @param  {ImageData} dataTextBuf
 *         pixel data for the accessible object with text visible.
 * @param  {ImageData} dataBackgroundBuf
 *         pixel data for the accessible object with transparent text.
 * @return {Object}
 *         RGBA or a range of RGBAs with min and max values.
 */
function getBgRGBA(dataTextBuf, dataBackgroundBuf) {
  let min = [0, 0, 0, 1];
  let max = [255, 255, 255, 1];
  let minLuminance = 1;
  let maxLuminance = 0;
  const luminances = {};
  const dataText = new Uint8ClampedArray(dataTextBuf);
  const dataBackground = new Uint8ClampedArray(dataBackgroundBuf);

  let foundDistinctColor = false;
  for (let i = 0; i < dataText.length; i = i + 4) {
    const tR = dataText[i];
    const bgR = dataBackground[i];
    const tG = dataText[i + 1];
    const bgG = dataBackground[i + 1];
    const tB = dataText[i + 2];
    const bgB = dataBackground[i + 2];

    // Ignore pixels that are the same where pixels that are different between the two
    // images are assumed to belong to the text within the node.
    if (tR === bgR && tG === bgG && tB === bgB) {
      continue;
    }

    foundDistinctColor = true;

    const bgColor = `rgb(${bgR}, ${bgG}, ${bgB})`;
    let luminance = luminances[bgColor];

    if (!luminance) {
      // Calculate luminance for the RGB value and store it to only measure once.
      luminance = calculateLuminance([bgR, bgG, bgB]);
      luminances[bgColor] = luminance;
    }

    if (minLuminance >= luminance) {
      minLuminance = luminance;
      min = [bgR, bgG, bgB, 1];
    }

    if (maxLuminance <= luminance) {
      maxLuminance = luminance;
      max = [bgR, bgG, bgB, 1];
    }
  }

  if (!foundDistinctColor) {
    return null;
  }

  return minLuminance === maxLuminance ? { value: max } : { min, max };
}
