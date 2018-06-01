/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test classifyColor.

"use strict";

var {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {colorUtils} = require("devtools/shared/css/color");
const InspectorUtils = require("InspectorUtils");

const CLASSIFY_TESTS = [
  { input: "rgb(255,0,192)", output: "rgb" },
  { input: "RGB(255,0,192)", output: "rgb" },
  { input: "RGB(100%,0%,83%)", output: "rgb" },
  { input: "rgba(255,0,192, 0.25)", output: "rgb" },
  { input: "hsl(5, 5%, 5%)", output: "hsl" },
  { input: "hsla(5, 5%, 5%, 0.25)", output: "hsl" },
  { input: "hSlA(5, 5%, 5%, 0.25)", output: "hsl" },
  { input: "#f06", output: "hex" },
  { input: "#f060", output: "hex" },
  { input: "#fe01cb", output: "hex" },
  { input: "#fe01cb80", output: "hex" },
  { input: "#FE01CB", output: "hex" },
  { input: "#FE01CB80", output: "hex" },
  { input: "blue", output: "name" },
  { input: "orange", output: "name" }
];

function compareWithInspectorUtils(input, isColor) {
  const ours = colorUtils.colorToRGBA(input);
  const platform = InspectorUtils.colorToRGBA(input);
  deepEqual(ours, platform, "color " + input + " matches InspectorUtils");
  if (isColor) {
    ok(ours !== null, "'" + input + "' is a color");
  } else {
    ok(ours === null, "'" + input + "' is not a color");
  }
}

function run_test() {
  for (const test of CLASSIFY_TESTS) {
    const result = colorUtils.classifyColor(test.input);
    equal(result, test.output, "test classifyColor(" + test.input + ")");

    const obj = new colorUtils.CssColor("purple");
    obj.setAuthoredUnitFromColor(test.input);
    equal(obj.colorUnit, test.output,
          "test setAuthoredUnitFromColor(" + test.input + ")");

    // Check that our implementation matches InspectorUtils.
    compareWithInspectorUtils(test.input, true);

    // And check some obvious errors.
    compareWithInspectorUtils("mumble" + test.input, false);
    compareWithInspectorUtils(test.input + "trailingstuff", false);
  }

  // Regression test for bug 1303826.
  const black = new colorUtils.CssColor("#000");
  black.colorUnit = "name";
  equal(black.toString(), "black", "test non-upper-case color cycling");

  const upper = new colorUtils.CssColor("BLACK");
  upper.colorUnit = "hex";
  equal(upper.toString(), "#000", "test upper-case color cycling");
  upper.colorUnit = "name";
  equal(upper.toString(), "BLACK", "test upper-case color preservation");
}
