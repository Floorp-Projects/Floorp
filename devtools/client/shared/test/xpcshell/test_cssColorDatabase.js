/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that css-color-db matches platform.

"use strict";

const { colorUtils } = require("resource://devtools/shared/css/color.js");
const { cssColors } = require("resource://devtools/shared/css/color-db.js");

function isValid(colorName) {
  ok(
    InspectorUtils.isValidCSSColor(colorName),
    colorName + " is valid in InspectorUtils"
  );
}

function checkOne(colorName, checkName) {
  const fromDom = InspectorUtils.colorToRGBA(colorName);

  isValid(colorName);

  if (checkName) {
    const { r, g, b } = fromDom;

    // The color we got might not map back to the same name; but our
    // implementation should agree with InspectorUtils about which name is
    // canonical.
    const ourName = colorUtils.rgbToColorName(r, g, b);
    const domName = InspectorUtils.rgbToColorName(r, g, b);

    equal(
      ourName,
      domName,
      colorName + " canonical name agrees with InspectorUtils"
    );
  }
}

function run_test() {
  for (const name in cssColors) {
    checkOne(name, true);
  }
  checkOne("transparent", false);
}
