/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test color cycling regression - Bug 1303748.
 *
 * Values should cycle from a starting value, back to their original values. This can
 * potentially be a little flaky due to the precision of different color representations.
 */

const { colorUtils } = require("resource://devtools/shared/css/color.js");
const getFixtureColorData = require("resource://test/helper_color_data.js");

function run_test() {
  getFixtureColorData().forEach(
    ({ authored, name, hex, hsl, rgb, hwb, cycle }) => {
      if (cycle) {
        const nameCycled = runCycle(name, cycle);
        const hexCycled = runCycle(hex, cycle);
        const hslCycled = runCycle(hsl, cycle);
        const rgbCycled = runCycle(rgb, cycle);
        const hwbCycled = runCycle(hwb, cycle);
        // Cut down on log output by only reporting a single pass/fail for the color.
        ok(
          nameCycled && hexCycled && hslCycled && rgbCycled && hwbCycled,
          `${authored} was able to cycle back to the original value`
        );
      }
    }
  );
}

/**
 * Test a color cycle to see if a color cycles back to its original value in a fixed
 * number of steps.
 *
 * @param {string} value - The color value, e.g. "#000".
 * @param {integer) times - The number of times it takes to cycle back to the
 *                          original color.
 */
function runCycle(value, times) {
  let color = new colorUtils.CssColor(value);
  const colorUnit = colorUtils.classifyColor(value);
  for (let i = 0; i < times; i++) {
    const newColor = color.nextColorUnit();
    color = new colorUtils.CssColor(newColor);
  }
  return color.toString(colorUnit) === value;
}
