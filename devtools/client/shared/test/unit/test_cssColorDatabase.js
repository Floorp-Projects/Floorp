/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that css-color-db matches platform.

"use strict";

var {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});

const {colorUtils} = require("devtools/shared/css/color");
const {cssColors} = require("devtools/shared/css/color-db");
const InspectorUtils = require("InspectorUtils");

function isValid(colorName) {
  ok(colorUtils.isValidCSSColor(colorName),
     colorName + " is valid in database");
  ok(InspectorUtils.isValidCSSColor(colorName),
     colorName + " is valid in InspectorUtils");
}

function checkOne(colorName, checkName) {
  let ours = colorUtils.colorToRGBA(colorName);
  let fromDom = InspectorUtils.colorToRGBA(colorName);
  deepEqual(ours, fromDom, colorName + " agrees with InspectorUtils");

  isValid(colorName);

  if (checkName) {
    let {r, g, b} = ours;

    // The color we got might not map back to the same name; but our
    // implementation should agree with InspectorUtils about which name is
    // canonical.
    let ourName = colorUtils.rgbToColorName(r, g, b);
    let domName = InspectorUtils.rgbToColorName(r, g, b);

    equal(ourName, domName,
          colorName + " canonical name agrees with InspectorUtils");
  }
}

function run_test() {
  for (let name in cssColors) {
    checkOne(name, true);
  }
  checkOne("transparent", false);

  // Now check that platform didn't add a new name when we weren't
  // looking.
  // XXX Disable this test for now as getCSSValuesForProperty no longer
  //     returns the complete color keyword list.
  if (false) {
    let names = InspectorUtils.getCSSValuesForProperty("background-color");
    for (let name of names) {
      if (name !== "hsl" && name !== "hsla" &&
          name !== "rgb" && name !== "rgba" &&
          name !== "inherit" && name !== "initial" && name !== "unset") {
        checkOne(name, true);
      }
    }
  }
}
