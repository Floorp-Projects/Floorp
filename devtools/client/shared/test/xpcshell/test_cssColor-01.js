/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test classifyColor.

"use strict";

const { colorUtils } = require("resource://devtools/shared/css/color.js");

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
  { input: "orange", output: "name" },
  // // Once bug 1824400 is closed, these types should be recognized
  // { input: "oklch(50% 0.3 180)", output: "oklch" },
  // { input: "oklch(50% 0.3 180 / 0.5)", output: "oklch" },
  // { input: "oklab(50% -0.3 0.3)", output: "oklab" },
  // { input: "oklab(50% -0.3 0.3 / 0.5)", output: "oklab" },
  // { input: "lch(50% 0.3 180)", output: "lch" },
  // { input: "lch(50% 0.3 180 / 0.5)", output: "lch" },
  // { input: "lab(50% -0.3 0.3)", output: "lab" },
  // { input: "lab(50% -0.3 0.3 / 0.5)", output: "lab" },
  // // But if they are not recognized, they should be classified as "authored"
  { input: "oklch(50% 0.3 180)", output: "authored" },
  { input: "oklch(50% 0.3 180 / 0.5)", output: "authored" },
  { input: "oklab(50% -0.3 0.3)", output: "authored" },
  { input: "oklab(50% -0.3 0.3 / 0.5)", output: "authored" },
  { input: "lch(50% 0.3 180)", output: "authored" },
  { input: "lch(50% 0.3 180 / 0.5)", output: "authored" },
  { input: "lab(50% -0.3 0.3)", output: "authored" },
  { input: "lab(50% -0.3 0.3 / 0.5)", output: "authored" },
];

function run_test() {
  for (const test of CLASSIFY_TESTS) {
    const result = colorUtils.classifyColor(test.input);
    equal(result, test.output, "test classifyColor(" + test.input + ")");

    ok(
      InspectorUtils.colorToRGBA(test.input) !== null,
      "'" + test.input + "' is a color"
    );

    // check some obvious errors.
    const invalidColors = ["mumble" + test.input, test.input + "trailingstuff"];
    for (const invalidColor of invalidColors) {
      ok(
        InspectorUtils.colorToRGBA(invalidColor) == null,
        `'${invalidColor}' is not a color`
      );
    }
  }

  // Regression test for bug 1303826.
  const black = new colorUtils.CssColor("#000");
  equal(black.toString("name"), "black", "test non-upper-case color cycling");

  const upper = new colorUtils.CssColor("BLACK");
  equal(upper.toString("hex"), "#000", "test upper-case color cycling");
  equal(upper.toString("name"), "BLACK", "test upper-case color preservation");
}
