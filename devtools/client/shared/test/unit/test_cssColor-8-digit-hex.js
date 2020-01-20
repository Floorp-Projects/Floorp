/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// 8 character hex colors have 256 possible alpha values compared to the
// standard 100 values possible via rgba() colors. This test ensures that they
// are stored correctly without any alpha loss.

"use strict";

var { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { colorUtils } = require("devtools/shared/css/color");

const EIGHT_CHARACTER_HEX = "#fefefef0";

// eslint-disable-next-line
function run_test() {
  const cssColor = new colorUtils.CssColor(EIGHT_CHARACTER_HEX);
  cssColor.colorUnit = colorUtils.CssColor.COLORUNIT.hex;

  const color = cssColor.toString();

  equal(color, EIGHT_CHARACTER_HEX, "alpha value is correct");
}
