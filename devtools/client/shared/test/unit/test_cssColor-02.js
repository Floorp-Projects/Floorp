/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test color cycling regression - Bug 1303748.
 *
 * Values should cycle from a starting value, back to their original values. This can
 * potentially be a little flaky due to the precision of different color representations.
 */

const {require} = Components.utils.import("resource://devtools/shared/Loader.jsm", {});
const {colorUtils} = require("devtools/shared/css/color");
const getFixtureColorData = require("resource://test/helper_color_data.js");

function run_test() {
  getFixtureColorData().forEach(({authored, name, hex, hsl, rgb, cycle}) => {
    if (cycle) {
      const nameCycled = runCycle(name, cycle);
      const hexCycled = runCycle(hex, cycle);
      const hslCycled = runCycle(hsl, cycle);
      const rgbCycled = runCycle(rgb, cycle);
      // Cut down on log output by only reporting a single pass/fail for the color.
      ok(nameCycled && hexCycled && hslCycled && rgbCycled,
        `${authored} was able to cycle back to the original value`);
    }
  });
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
  for (let i = 0; i < times; i++) {
    color.nextColorUnit();
    color = new colorUtils.CssColor(color.toString());
  }
  return color.toString() === value;
}
