/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test css-color-4 color function syntax and old-style syntax.

"use strict";

var Cu = Components.utils;
var Ci = Components.interfaces;
var Cc = Components.classes;

var {require, loader} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {colorUtils} = require("devtools/shared/css/color");

loader.lazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

const OLD_STYLE_TESTS = [
  "rgb(255,0,192)",
  "RGB(255,0,192)",
  "RGB(100%,0%,83%)",
  "rgba(255,0,192,0.25)",
  "hsl(120, 100%, 40%)",
  "hsla(120, 100%, 40%, 0.25)",
  "hSlA(240, 100%, 50%, 0.25)",
];

const CSS_COLOR_4_TESTS = [
  "rgb(255.0,0.0,192.0)",
  "RGB(255 0 192)",
  "RGB(100% 0% 83% / 0.5)",
  "RGB(100%,0%,83%,0.5)",
  "RGB(100%,0%,83%,50%)",
  "rgba(255,0,192)",
  "hsl(50deg,15%,25%)",
  "hsl(240 25% 33%)",
  "hsl(50deg 25% 33% / 0.25)",
  "hsl(60 120% 60% / 0.25)",
  "hSlA(5turn 40% 4%)",
];

function run_test() {
  for (let test of OLD_STYLE_TESTS) {
    let ours = colorUtils.colorToRGBA(test, true);
    let platform = DOMUtils.colorToRGBA(test);
    deepEqual(ours, platform, "color " + test + " matches DOMUtils");
    ok(ours !== null, "'" + test + "' is a color");
  }

  for (let test of CSS_COLOR_4_TESTS) {
    let oursOld = colorUtils.colorToRGBA(test, true);
    let oursNew = colorUtils.colorToRGBA(test, false);
    let platform = DOMUtils.colorToRGBA(test);
    notEqual(oursOld, platform, "old style parser for color " + test +
             " should not match DOMUtils");
    ok(oursOld === null, "'" + test + "' is not a color with old parser");
    deepEqual(oursNew, platform, "css-color-4 parser for color " + test + " matches DOMUtils");
    ok(oursNew !== null, "'" + test + "' is a color with css-color-4 parser");
  }
}
