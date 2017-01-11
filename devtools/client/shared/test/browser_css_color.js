/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,browser_css_color.js";
var {colorUtils} = require("devtools/shared/css/color");
/* global getFixtureColorData */
loadHelperScript("helper_color_data.js");

add_task(function* () {
  yield addTab("about:blank");
  let [host,, doc] = yield createHost("bottom", TEST_URI);

  info("Creating a test canvas element to test colors");
  let canvas = createTestCanvas(doc);
  info("Starting the test");
  testColorUtils(canvas);

  host.destroy();
  gBrowser.removeCurrentTab();
});

function createTestCanvas(doc) {
  let canvas = doc.createElement("canvas");
  canvas.width = canvas.height = 10;
  doc.body.appendChild(canvas);
  return canvas;
}

function testColorUtils(canvas) {
  let data = getFixtureColorData();

  for (let {authored, name, hex, hsl, rgb} of data) {
    let color = new colorUtils.CssColor(authored);

    // Check all values.
    info("Checking values for " + authored);
    is(color.name, name, "color.name === name");
    is(color.hex, hex, "color.hex === hex");
    is(color.hsl, hsl, "color.hsl === hsl");
    is(color.rgb, rgb, "color.rgb === rgb");

    testToString(color, name, hex, hsl, rgb);
    testColorMatch(name, hex, hsl, rgb, color.rgba, canvas);
  }

  testSetAlpha();
}

function testToString(color, name, hex, hsl, rgb) {
  color.colorUnit = colorUtils.CssColor.COLORUNIT.name;
  is(color.toString(), name, "toString() with authored type");

  color.colorUnit = colorUtils.CssColor.COLORUNIT.hex;
  is(color.toString(), hex, "toString() with hex type");

  color.colorUnit = colorUtils.CssColor.COLORUNIT.hsl;
  is(color.toString(), hsl, "toString() with hsl type");

  color.colorUnit = colorUtils.CssColor.COLORUNIT.rgb;
  is(color.toString(), rgb, "toString() with rgb type");
}

function testColorMatch(name, hex, hsl, rgb, rgba, canvas) {
  let target;
  let ctx = canvas.getContext("2d");

  let clearCanvas = function () {
    canvas.width = 1;
  };
  let setColor = function (color) {
    ctx.fillStyle = color;
    ctx.fillRect(0, 0, 1, 1);
  };
  let setTargetColor = function () {
    clearCanvas();
    // All colors have rgba so we can use this to compare against.
    setColor(rgba);
    let [r, g, b, a] = ctx.getImageData(0, 0, 1, 1).data;
    target = {r: r, g: g, b: b, a: a};
  };
  let test = function (color, type) {
    // hsla -> rgba -> hsla produces inaccurate results so we
    // need some tolerence here.
    let tolerance = 3;
    clearCanvas();

    setColor(color);
    let [r, g, b, a] = ctx.getImageData(0, 0, 1, 1).data;

    let rgbFail = Math.abs(r - target.r) > tolerance ||
                  Math.abs(g - target.g) > tolerance ||
                  Math.abs(b - target.b) > tolerance;
    ok(!rgbFail, "color " + rgba + " matches target. Type: " + type);
    if (rgbFail) {
      info(`target: ${target.toSource()}, color: [r: ${r}, g: ${g}, b: ${b}, a: ${a}]`);
    }

    let alphaFail = a !== target.a;
    ok(!alphaFail, "color " + rgba + " alpha value matches target.");
  };

  setTargetColor();

  test(name, "name");
  test(hex, "hex");
  test(hsl, "hsl");
  test(rgb, "rgb");
}

function testSetAlpha() {
  let values = [
    ["longhex", "#ff0000", 0.5, "rgba(255, 0, 0, 0.5)"],
    ["hex", "#f0f", 0.2, "rgba(255, 0, 255, 0.2)"],
    ["rgba", "rgba(120, 34, 23, 1)", 0.25, "rgba(120, 34, 23, 0.25)"],
    ["rgb", "rgb(120, 34, 23)", 0.25, "rgba(120, 34, 23, 0.25)"],
    ["hsl", "hsl(208, 100%, 97%)", 0.75, "rgba(240, 248, 255, 0.75)"],
    ["hsla", "hsla(208, 100%, 97%, 1)", 0.75, "rgba(240, 248, 255, 0.75)"],
    ["alphahex", "#f08f", 0.6, "rgba(255, 0, 136, 0.6)"],
    ["longalphahex", "#00ff80ff", 0.2, "rgba(0, 255, 128, 0.2)"]
  ];
  values.forEach(([type, value, alpha, expected]) => {
    is(colorUtils.setAlpha(value, alpha), expected,
       "correctly sets alpha value for " + type);
  });

  try {
    colorUtils.setAlpha("rgb(24, 25%, 45, 1)", 1);
    ok(false, "Should fail when passing in an invalid color.");
  } catch (e) {
    ok(true, "Fails when setAlpha receives an invalid color.");
  }

  is(colorUtils.setAlpha("#fff"), "rgba(255, 255, 255, 1)",
     "sets alpha to 1 if invalid.");
}
