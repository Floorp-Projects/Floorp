/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that css-color-db matches platform.

"use strict";

var Cu = Components.utils;
var Ci = Components.interfaces;
var Cc = Components.classes;

var {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});

const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);

const {colorUtils} = require("devtools/shared/css/color");
const {cssColors} = require("devtools/shared/css/color-db");

function isValid(colorName) {
  ok(colorUtils.isValidCSSColor(colorName),
     colorName + " is valid in database");
  ok(DOMUtils.isValidCSSColor(colorName),
     colorName + " is valid in DOMUtils");
}

function checkOne(colorName, checkName) {
  let ours = colorUtils.colorToRGBA(colorName);
  let fromDom = DOMUtils.colorToRGBA(colorName);
  deepEqual(ours, fromDom, colorName + " agrees with DOMUtils");

  isValid(colorName);

  if (checkName) {
    let {r, g, b} = ours;

    // The color we got might not map back to the same name; but our
    // implementation should agree with DOMUtils about which name is
    // canonical.
    let ourName = colorUtils.rgbToColorName(r, g, b);
    let domName = DOMUtils.rgbToColorName(r, g, b);

    equal(ourName, domName,
          colorName + " canonical name agrees with DOMUtils");
  }
}

function run_test() {
  for (let name in cssColors) {
    checkOne(name, true);
  }
  checkOne("transparent", false);

  // Now check that platform didn't add a new name when we weren't
  // looking.
  let names = DOMUtils.getCSSValuesForProperty("background-color");
  for (let name of names) {
    if (name !== "hsl" && name !== "hsla" &&
        name !== "rgb" && name !== "rgba" &&
        name !== "inherit" && name !== "initial" && name !== "unset") {
      checkOne(name, true);
    }
  }
}
