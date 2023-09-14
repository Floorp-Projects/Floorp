/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { colorUtils } = require("resource://devtools/shared/css/color.js");
/* global getFixtureColorData */
loadHelperScript("helper_color_data.js");

add_task(async function () {
  await addTab("about:blank");
  const { host, doc } = await createHost("bottom");

  info("Creating a test canvas element to test colors");
  const canvas = createTestCanvas(doc);
  info("Starting the test");
  testColorUtils(canvas);

  host.destroy();
  gBrowser.removeCurrentTab();
});

function createTestCanvas(doc) {
  const canvas = doc.createElement("canvas");
  canvas.width = canvas.height = 10;
  doc.body.appendChild(canvas);
  return canvas;
}

function testColorUtils(canvas) {
  const data = getFixtureColorData();

  for (const { authored, name, hex, hsl, rgb } of data) {
    const color = new colorUtils.CssColor(authored);

    // Check all values.
    info("Checking values for " + authored);
    is(color.name, name, "color.name === name");
    is(color.hex, hex, "color.hex === hex");
    is(color.hsl, hsl, "color.hsl === hsl");
    is(color.rgb, rgb, "color.rgb === rgb");

    testToString(color, name, hex, hsl, rgb);
    testColorMatch(name, hex, hsl, rgb, color.rgba, canvas);
  }
}

function testToString(color, name, hex, hsl, rgb) {
  const { COLORUNIT } = colorUtils.CssColor;
  is(color.toString(COLORUNIT.name), name, "toString() with authored type");
  is(color.toString(COLORUNIT.hex), hex, "toString() with hex type");
  is(color.toString(COLORUNIT.hsl), hsl, "toString() with hsl type");
  is(color.toString(COLORUNIT.rgb), rgb, "toString() with rgb type");
}

function testColorMatch(name, hex, hsl, rgb, rgba, canvas) {
  let target;
  const ctx = canvas.getContext("2d");

  const clearCanvas = function () {
    canvas.width = 1;
  };
  const setColor = function (color) {
    ctx.fillStyle = color;
    ctx.fillRect(0, 0, 1, 1);
  };
  const setTargetColor = function () {
    clearCanvas();
    // All colors have rgba so we can use this to compare against.
    setColor(rgba);
    const [r, g, b, a] = ctx.getImageData(0, 0, 1, 1).data;
    target = { r, g, b, a };
  };
  const test = function (color, type) {
    // hsla -> rgba -> hsla produces inaccurate results so we
    // need some tolerence here.
    const tolerance = 3;
    clearCanvas();

    setColor(color);
    const [r, g, b, a] = ctx.getImageData(0, 0, 1, 1).data;

    const rgbFail =
      Math.abs(r - target.r) > tolerance ||
      Math.abs(g - target.g) > tolerance ||
      Math.abs(b - target.b) > tolerance;
    ok(!rgbFail, "color " + rgba + " matches target. Type: " + type);
    if (rgbFail) {
      info(
        `target: ${JSON.stringify(
          target
        )}, color: [r: ${r}, g: ${g}, b: ${b}, a: ${a}]`
      );
    }

    const alphaFail = a !== target.a;
    ok(!alphaFail, "color " + rgba + " alpha value matches target.");
  };

  setTargetColor();

  test(name, "name");
  test(hex, "hex");
  test(hsl, "hsl");
  test(rgb, "rgb");
}
