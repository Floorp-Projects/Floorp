/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,browser_css_color.js";
var {colorUtils} = require("devtools/client/shared/css-color");
var origColorUnit;

add_task(function* () {
  yield addTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

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
  let data = getTestData();

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
  let setColor = function (aColor) {
    ctx.fillStyle = aColor;
    ctx.fillRect(0, 0, 1, 1);
  };
  let setTargetColor = function () {
    clearCanvas();
    // All colors have rgba so we can use this to compare against.
    setColor(rgba);
    let [r, g, b, a] = ctx.getImageData(0, 0, 1, 1).data;
    target = {r: r, g: g, b: b, a: a};
  };
  let test = function (aColor, type) {
    let tolerance = 3; // hsla -> rgba -> hsla produces inaccurate results so we
                       // need some tolerence here.
    clearCanvas();

    setColor(aColor);
    let [r, g, b, a] = ctx.getImageData(0, 0, 1, 1).data;

    let rgbFail = Math.abs(r - target.r) > tolerance ||
                  Math.abs(g - target.g) > tolerance ||
                  Math.abs(b - target.b) > tolerance;
    ok(!rgbFail, "color " + rgba + " matches target. Type: " + type);
    if (rgbFail) {
      info("target: " + (target.toSource()) + ", color: [r: " + r + ", g: " + g + ", b: " + b + ", a: " + a + "]");
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
    ["hsl", "hsl(208, 100%, 97%)", 0.75, "rgba(239, 247, 255, 0.75)"],
    ["hsla", "hsla(208, 100%, 97%, 1)", 0.75, "rgba(239, 247, 255, 0.75)"],
    ["alphahex", "#f08f", 0.6, "rgba(255, 0, 136, 0.6)"],
    ["longalphahex", "#00ff80ff", 0.2, "rgba(0, 255, 128, 0.2)"]
  ];
  values.forEach(([type, value, alpha, expected]) => {
    is(colorUtils.setAlpha(value, alpha), expected, "correctly sets alpha value for " + type);
  });

  try {
    colorUtils.setAlpha("rgb(24, 25, 45, 1)", 1);
    ok(false, "Should fail when passing in an invalid color.");
  } catch (e) {
    ok(true, "Fails when setAlpha receives an invalid color.");
  }

  is(colorUtils.setAlpha("#fff"), "rgba(255, 255, 255, 1)", "sets alpha to 1 if invalid.");
}

function getTestData() {
  return [
    {authored: "aliceblue", name: "aliceblue", hex: "#f0f8ff", hsl: "hsl(208, 100%, 97%)", rgb: "rgb(240, 248, 255)"},
    {authored: "antiquewhite", name: "antiquewhite", hex: "#faebd7", hsl: "hsl(34, 78%, 91%)", rgb: "rgb(250, 235, 215)"},
    {authored: "aqua", name: "aqua", hex: "#0ff", hsl: "hsl(180, 100%, 50%)", rgb: "rgb(0, 255, 255)"},
    {authored: "aquamarine", name: "aquamarine", hex: "#7fffd4", hsl: "hsl(160, 100%, 75%)", rgb: "rgb(127, 255, 212)"},
    {authored: "azure", name: "azure", hex: "#f0ffff", hsl: "hsl(180, 100%, 97%)", rgb: "rgb(240, 255, 255)"},
    {authored: "beige", name: "beige", hex: "#f5f5dc", hsl: "hsl(60, 56%, 91%)", rgb: "rgb(245, 245, 220)"},
    {authored: "bisque", name: "bisque", hex: "#ffe4c4", hsl: "hsl(33, 100%, 88%)", rgb: "rgb(255, 228, 196)"},
    {authored: "black", name: "black", hex: "#000", hsl: "hsl(0, 0%, 0%)", rgb: "rgb(0, 0, 0)"},
    {authored: "blanchedalmond", name: "blanchedalmond", hex: "#ffebcd", hsl: "hsl(36, 100%, 90%)", rgb: "rgb(255, 235, 205)"},
    {authored: "blue", name: "blue", hex: "#00f", hsl: "hsl(240, 100%, 50%)", rgb: "rgb(0, 0, 255)"},
    {authored: "blueviolet", name: "blueviolet", hex: "#8a2be2", hsl: "hsl(271, 76%, 53%)", rgb: "rgb(138, 43, 226)"},
    {authored: "brown", name: "brown", hex: "#a52a2a", hsl: "hsl(0, 59%, 41%)", rgb: "rgb(165, 42, 42)"},
    {authored: "burlywood", name: "burlywood", hex: "#deb887", hsl: "hsl(34, 57%, 70%)", rgb: "rgb(222, 184, 135)"},
    {authored: "cadetblue", name: "cadetblue", hex: "#5f9ea0", hsl: "hsl(182, 25%, 50%)", rgb: "rgb(95, 158, 160)"},
    {authored: "chartreuse", name: "chartreuse", hex: "#7fff00", hsl: "hsl(90, 100%, 50%)", rgb: "rgb(127, 255, 0)"},
    {authored: "chocolate", name: "chocolate", hex: "#d2691e", hsl: "hsl(25, 75%, 47%)", rgb: "rgb(210, 105, 30)"},
    {authored: "coral", name: "coral", hex: "#ff7f50", hsl: "hsl(16, 100%, 66%)", rgb: "rgb(255, 127, 80)"},
    {authored: "cornflowerblue", name: "cornflowerblue", hex: "#6495ed", hsl: "hsl(219, 79%, 66%)", rgb: "rgb(100, 149, 237)"},
    {authored: "cornsilk", name: "cornsilk", hex: "#fff8dc", hsl: "hsl(48, 100%, 93%)", rgb: "rgb(255, 248, 220)"},
    {authored: "crimson", name: "crimson", hex: "#dc143c", hsl: "hsl(348, 83%, 47%)", rgb: "rgb(220, 20, 60)"},
    {authored: "cyan", name: "aqua", hex: "#0ff", hsl: "hsl(180, 100%, 50%)", rgb: "rgb(0, 255, 255)"},
    {authored: "darkblue", name: "darkblue", hex: "#00008b", hsl: "hsl(240, 100%, 27%)", rgb: "rgb(0, 0, 139)"},
    {authored: "darkcyan", name: "darkcyan", hex: "#008b8b", hsl: "hsl(180, 100%, 27%)", rgb: "rgb(0, 139, 139)"},
    {authored: "darkgoldenrod", name: "darkgoldenrod", hex: "#b8860b", hsl: "hsl(43, 89%, 38%)", rgb: "rgb(184, 134, 11)"},
    {authored: "darkgray", name: "darkgray", hex: "#a9a9a9", hsl: "hsl(0, 0%, 66%)", rgb: "rgb(169, 169, 169)"},
    {authored: "darkgreen", name: "darkgreen", hex: "#006400", hsl: "hsl(120, 100%, 20%)", rgb: "rgb(0, 100, 0)"},
    {authored: "darkgrey", name: "darkgray", hex: "#a9a9a9", hsl: "hsl(0, 0%, 66%)", rgb: "rgb(169, 169, 169)"},
    {authored: "darkkhaki", name: "darkkhaki", hex: "#bdb76b", hsl: "hsl(56, 38%, 58%)", rgb: "rgb(189, 183, 107)"},
    {authored: "darkmagenta", name: "darkmagenta", hex: "#8b008b", hsl: "hsl(300, 100%, 27%)", rgb: "rgb(139, 0, 139)"},
    {authored: "darkolivegreen", name: "darkolivegreen", hex: "#556b2f", hsl: "hsl(82, 39%, 30%)", rgb: "rgb(85, 107, 47)"},
    {authored: "darkorange", name: "darkorange", hex: "#ff8c00", hsl: "hsl(33, 100%, 50%)", rgb: "rgb(255, 140, 0)"},
    {authored: "darkorchid", name: "darkorchid", hex: "#9932cc", hsl: "hsl(280, 61%, 50%)", rgb: "rgb(153, 50, 204)"},
    {authored: "darkred", name: "darkred", hex: "#8b0000", hsl: "hsl(0, 100%, 27%)", rgb: "rgb(139, 0, 0)"},
    {authored: "darksalmon", name: "darksalmon", hex: "#e9967a", hsl: "hsl(15, 72%, 70%)", rgb: "rgb(233, 150, 122)"},
    {authored: "darkseagreen", name: "darkseagreen", hex: "#8fbc8f", hsl: "hsl(120, 25%, 65%)", rgb: "rgb(143, 188, 143)"},
    {authored: "darkslateblue", name: "darkslateblue", hex: "#483d8b", hsl: "hsl(248, 39%, 39%)", rgb: "rgb(72, 61, 139)"},
    {authored: "darkslategray", name: "darkslategray", hex: "#2f4f4f", hsl: "hsl(180, 25%, 25%)", rgb: "rgb(47, 79, 79)"},
    {authored: "darkslategrey", name: "darkslategray", hex: "#2f4f4f", hsl: "hsl(180, 25%, 25%)", rgb: "rgb(47, 79, 79)"},
    {authored: "darkturquoise", name: "darkturquoise", hex: "#00ced1", hsl: "hsl(181, 100%, 41%)", rgb: "rgb(0, 206, 209)"},
    {authored: "darkviolet", name: "darkviolet", hex: "#9400d3", hsl: "hsl(282, 100%, 41%)", rgb: "rgb(148, 0, 211)"},
    {authored: "deeppink", name: "deeppink", hex: "#ff1493", hsl: "hsl(328, 100%, 54%)", rgb: "rgb(255, 20, 147)"},
    {authored: "deepskyblue", name: "deepskyblue", hex: "#00bfff", hsl: "hsl(195, 100%, 50%)", rgb: "rgb(0, 191, 255)"},
    {authored: "dimgray", name: "dimgray", hex: "#696969", hsl: "hsl(0, 0%, 41%)", rgb: "rgb(105, 105, 105)"},
    {authored: "dodgerblue", name: "dodgerblue", hex: "#1e90ff", hsl: "hsl(210, 100%, 56%)", rgb: "rgb(30, 144, 255)"},
    {authored: "firebrick", name: "firebrick", hex: "#b22222", hsl: "hsl(0, 68%, 42%)", rgb: "rgb(178, 34, 34)"},
    {authored: "floralwhite", name: "floralwhite", hex: "#fffaf0", hsl: "hsl(40, 100%, 97%)", rgb: "rgb(255, 250, 240)"},
    {authored: "forestgreen", name: "forestgreen", hex: "#228b22", hsl: "hsl(120, 61%, 34%)", rgb: "rgb(34, 139, 34)"},
    {authored: "fuchsia", name: "fuchsia", hex: "#f0f", hsl: "hsl(300, 100%, 50%)", rgb: "rgb(255, 0, 255)"},
    {authored: "gainsboro", name: "gainsboro", hex: "#dcdcdc", hsl: "hsl(0, 0%, 86%)", rgb: "rgb(220, 220, 220)"},
    {authored: "ghostwhite", name: "ghostwhite", hex: "#f8f8ff", hsl: "hsl(240, 100%, 99%)", rgb: "rgb(248, 248, 255)"},
    {authored: "gold", name: "gold", hex: "#ffd700", hsl: "hsl(51, 100%, 50%)", rgb: "rgb(255, 215, 0)"},
    {authored: "goldenrod", name: "goldenrod", hex: "#daa520", hsl: "hsl(43, 74%, 49%)", rgb: "rgb(218, 165, 32)"},
    {authored: "gray", name: "gray", hex: "#808080", hsl: "hsl(0, 0%, 50%)", rgb: "rgb(128, 128, 128)"},
    {authored: "green", name: "green", hex: "#008000", hsl: "hsl(120, 100%, 25%)", rgb: "rgb(0, 128, 0)"},
    {authored: "greenyellow", name: "greenyellow", hex: "#adff2f", hsl: "hsl(84, 100%, 59%)", rgb: "rgb(173, 255, 47)"},
    {authored: "grey", name: "gray", hex: "#808080", hsl: "hsl(0, 0%, 50%)", rgb: "rgb(128, 128, 128)"},
    {authored: "honeydew", name: "honeydew", hex: "#f0fff0", hsl: "hsl(120, 100%, 97%)", rgb: "rgb(240, 255, 240)"},
    {authored: "hotpink", name: "hotpink", hex: "#ff69b4", hsl: "hsl(330, 100%, 71%)", rgb: "rgb(255, 105, 180)"},
    {authored: "indianred", name: "indianred", hex: "#cd5c5c", hsl: "hsl(0, 53%, 58%)", rgb: "rgb(205, 92, 92)"},
    {authored: "indigo", name: "indigo", hex: "#4b0082", hsl: "hsl(275, 100%, 25%)", rgb: "rgb(75, 0, 130)"},
    {authored: "ivory", name: "ivory", hex: "#fffff0", hsl: "hsl(60, 100%, 97%)", rgb: "rgb(255, 255, 240)"},
    {authored: "khaki", name: "khaki", hex: "#f0e68c", hsl: "hsl(54, 77%, 75%)", rgb: "rgb(240, 230, 140)"},
    {authored: "lavender", name: "lavender", hex: "#e6e6fa", hsl: "hsl(240, 67%, 94%)", rgb: "rgb(230, 230, 250)"},
    {authored: "lavenderblush", name: "lavenderblush", hex: "#fff0f5", hsl: "hsl(340, 100%, 97%)", rgb: "rgb(255, 240, 245)"},
    {authored: "lawngreen", name: "lawngreen", hex: "#7cfc00", hsl: "hsl(90, 100%, 49%)", rgb: "rgb(124, 252, 0)"},
    {authored: "lemonchiffon", name: "lemonchiffon", hex: "#fffacd", hsl: "hsl(54, 100%, 90%)", rgb: "rgb(255, 250, 205)"},
    {authored: "lightblue", name: "lightblue", hex: "#add8e6", hsl: "hsl(195, 53%, 79%)", rgb: "rgb(173, 216, 230)"},
    {authored: "lightcoral", name: "lightcoral", hex: "#f08080", hsl: "hsl(0, 79%, 72%)", rgb: "rgb(240, 128, 128)"},
    {authored: "lightcyan", name: "lightcyan", hex: "#e0ffff", hsl: "hsl(180, 100%, 94%)", rgb: "rgb(224, 255, 255)"},
    {authored: "lightgoldenrodyellow", name: "lightgoldenrodyellow", hex: "#fafad2", hsl: "hsl(60, 80%, 90%)", rgb: "rgb(250, 250, 210)"},
    {authored: "lightgray", name: "lightgray", hex: "#d3d3d3", hsl: "hsl(0, 0%, 83%)", rgb: "rgb(211, 211, 211)"},
    {authored: "lightgreen", name: "lightgreen", hex: "#90ee90", hsl: "hsl(120, 73%, 75%)", rgb: "rgb(144, 238, 144)"},
    {authored: "lightgrey", name: "lightgray", hex: "#d3d3d3", hsl: "hsl(0, 0%, 83%)", rgb: "rgb(211, 211, 211)"},
    {authored: "lightpink", name: "lightpink", hex: "#ffb6c1", hsl: "hsl(351, 100%, 86%)", rgb: "rgb(255, 182, 193)"},
    {authored: "lightsalmon", name: "lightsalmon", hex: "#ffa07a", hsl: "hsl(17, 100%, 74%)", rgb: "rgb(255, 160, 122)"},
    {authored: "lightseagreen", name: "lightseagreen", hex: "#20b2aa", hsl: "hsl(177, 70%, 41%)", rgb: "rgb(32, 178, 170)"},
    {authored: "lightskyblue", name: "lightskyblue", hex: "#87cefa", hsl: "hsl(203, 92%, 75%)", rgb: "rgb(135, 206, 250)"},
    {authored: "lightslategray", name: "lightslategray", hex: "#789", hsl: "hsl(210, 14%, 53%)", rgb: "rgb(119, 136, 153)"},
    {authored: "lightslategrey", name: "lightslategray", hex: "#789", hsl: "hsl(210, 14%, 53%)", rgb: "rgb(119, 136, 153)"},
    {authored: "lightsteelblue", name: "lightsteelblue", hex: "#b0c4de", hsl: "hsl(214, 41%, 78%)", rgb: "rgb(176, 196, 222)"},
    {authored: "lightyellow", name: "lightyellow", hex: "#ffffe0", hsl: "hsl(60, 100%, 94%)", rgb: "rgb(255, 255, 224)"},
    {authored: "lime", name: "lime", hex: "#0f0", hsl: "hsl(120, 100%, 50%)", rgb: "rgb(0, 255, 0)"},
    {authored: "limegreen", name: "limegreen", hex: "#32cd32", hsl: "hsl(120, 61%, 50%)", rgb: "rgb(50, 205, 50)"},
    {authored: "linen", name: "linen", hex: "#faf0e6", hsl: "hsl(30, 67%, 94%)", rgb: "rgb(250, 240, 230)"},
    {authored: "magenta", name: "fuchsia", hex: "#f0f", hsl: "hsl(300, 100%, 50%)", rgb: "rgb(255, 0, 255)"},
    {authored: "maroon", name: "maroon", hex: "#800000", hsl: "hsl(0, 100%, 25%)", rgb: "rgb(128, 0, 0)"},
    {authored: "mediumaquamarine", name: "mediumaquamarine", hex: "#66cdaa", hsl: "hsl(160, 51%, 60%)", rgb: "rgb(102, 205, 170)"},
    {authored: "mediumblue", name: "mediumblue", hex: "#0000cd", hsl: "hsl(240, 100%, 40%)", rgb: "rgb(0, 0, 205)"},
    {authored: "mediumorchid", name: "mediumorchid", hex: "#ba55d3", hsl: "hsl(288, 59%, 58%)", rgb: "rgb(186, 85, 211)"},
    {authored: "mediumpurple", name: "mediumpurple", hex: "#9370db", hsl: "hsl(260, 60%, 65%)", rgb: "rgb(147, 112, 219)"},
    {authored: "mediumseagreen", name: "mediumseagreen", hex: "#3cb371", hsl: "hsl(147, 50%, 47%)", rgb: "rgb(60, 179, 113)"},
    {authored: "mediumslateblue", name: "mediumslateblue", hex: "#7b68ee", hsl: "hsl(249, 80%, 67%)", rgb: "rgb(123, 104, 238)"},
    {authored: "mediumspringgreen", name: "mediumspringgreen", hex: "#00fa9a", hsl: "hsl(157, 100%, 49%)", rgb: "rgb(0, 250, 154)"},
    {authored: "mediumturquoise", name: "mediumturquoise", hex: "#48d1cc", hsl: "hsl(178, 60%, 55%)", rgb: "rgb(72, 209, 204)"},
    {authored: "mediumvioletred", name: "mediumvioletred", hex: "#c71585", hsl: "hsl(322, 81%, 43%)", rgb: "rgb(199, 21, 133)"},
    {authored: "midnightblue", name: "midnightblue", hex: "#191970", hsl: "hsl(240, 64%, 27%)", rgb: "rgb(25, 25, 112)"},
    {authored: "mintcream", name: "mintcream", hex: "#f5fffa", hsl: "hsl(150, 100%, 98%)", rgb: "rgb(245, 255, 250)"},
    {authored: "mistyrose", name: "mistyrose", hex: "#ffe4e1", hsl: "hsl(6, 100%, 94%)", rgb: "rgb(255, 228, 225)"},
    {authored: "moccasin", name: "moccasin", hex: "#ffe4b5", hsl: "hsl(38, 100%, 85%)", rgb: "rgb(255, 228, 181)"},
    {authored: "navajowhite", name: "navajowhite", hex: "#ffdead", hsl: "hsl(36, 100%, 84%)", rgb: "rgb(255, 222, 173)"},
    {authored: "navy", name: "navy", hex: "#000080", hsl: "hsl(240, 100%, 25%)", rgb: "rgb(0, 0, 128)"},
    {authored: "oldlace", name: "oldlace", hex: "#fdf5e6", hsl: "hsl(39, 85%, 95%)", rgb: "rgb(253, 245, 230)"},
    {authored: "olive", name: "olive", hex: "#808000", hsl: "hsl(60, 100%, 25%)", rgb: "rgb(128, 128, 0)"},
    {authored: "olivedrab", name: "olivedrab", hex: "#6b8e23", hsl: "hsl(80, 60%, 35%)", rgb: "rgb(107, 142, 35)"},
    {authored: "orange", name: "orange", hex: "#ffa500", hsl: "hsl(39, 100%, 50%)", rgb: "rgb(255, 165, 0)"},
    {authored: "orangered", name: "orangered", hex: "#ff4500", hsl: "hsl(16, 100%, 50%)", rgb: "rgb(255, 69, 0)"},
    {authored: "orchid", name: "orchid", hex: "#da70d6", hsl: "hsl(302, 59%, 65%)", rgb: "rgb(218, 112, 214)"},
    {authored: "palegoldenrod", name: "palegoldenrod", hex: "#eee8aa", hsl: "hsl(55, 67%, 80%)", rgb: "rgb(238, 232, 170)"},
    {authored: "palegreen", name: "palegreen", hex: "#98fb98", hsl: "hsl(120, 93%, 79%)", rgb: "rgb(152, 251, 152)"},
    {authored: "paleturquoise", name: "paleturquoise", hex: "#afeeee", hsl: "hsl(180, 65%, 81%)", rgb: "rgb(175, 238, 238)"},
    {authored: "palevioletred", name: "palevioletred", hex: "#db7093", hsl: "hsl(340, 60%, 65%)", rgb: "rgb(219, 112, 147)"},
    {authored: "papayawhip", name: "papayawhip", hex: "#ffefd5", hsl: "hsl(37, 100%, 92%)", rgb: "rgb(255, 239, 213)"},
    {authored: "peachpuff", name: "peachpuff", hex: "#ffdab9", hsl: "hsl(28, 100%, 86%)", rgb: "rgb(255, 218, 185)"},
    {authored: "peru", name: "peru", hex: "#cd853f", hsl: "hsl(30, 59%, 53%)", rgb: "rgb(205, 133, 63)"},
    {authored: "pink", name: "pink", hex: "#ffc0cb", hsl: "hsl(350, 100%, 88%)", rgb: "rgb(255, 192, 203)"},
    {authored: "plum", name: "plum", hex: "#dda0dd", hsl: "hsl(300, 47%, 75%)", rgb: "rgb(221, 160, 221)"},
    {authored: "powderblue", name: "powderblue", hex: "#b0e0e6", hsl: "hsl(187, 52%, 80%)", rgb: "rgb(176, 224, 230)"},
    {authored: "purple", name: "purple", hex: "#800080", hsl: "hsl(300, 100%, 25%)", rgb: "rgb(128, 0, 128)"},
    {authored: "rebeccapurple", name: "rebeccapurple", hex: "#639", hsl: "hsl(270, 50%, 40%)", rgb: "rgb(102, 51, 153)"},
    {authored: "red", name: "red", hex: "#f00", hsl: "hsl(0, 100%, 50%)", rgb: "rgb(255, 0, 0)"},
    {authored: "rosybrown", name: "rosybrown", hex: "#bc8f8f", hsl: "hsl(0, 25%, 65%)", rgb: "rgb(188, 143, 143)"},
    {authored: "royalblue", name: "royalblue", hex: "#4169e1", hsl: "hsl(225, 73%, 57%)", rgb: "rgb(65, 105, 225)"},
    {authored: "saddlebrown", name: "saddlebrown", hex: "#8b4513", hsl: "hsl(25, 76%, 31%)", rgb: "rgb(139, 69, 19)"},
    {authored: "salmon", name: "salmon", hex: "#fa8072", hsl: "hsl(6, 93%, 71%)", rgb: "rgb(250, 128, 114)"},
    {authored: "sandybrown", name: "sandybrown", hex: "#f4a460", hsl: "hsl(28, 87%, 67%)", rgb: "rgb(244, 164, 96)"},
    {authored: "seagreen", name: "seagreen", hex: "#2e8b57", hsl: "hsl(146, 50%, 36%)", rgb: "rgb(46, 139, 87)"},
    {authored: "seashell", name: "seashell", hex: "#fff5ee", hsl: "hsl(25, 100%, 97%)", rgb: "rgb(255, 245, 238)"},
    {authored: "sienna", name: "sienna", hex: "#a0522d", hsl: "hsl(19, 56%, 40%)", rgb: "rgb(160, 82, 45)"},
    {authored: "silver", name: "silver", hex: "#c0c0c0", hsl: "hsl(0, 0%, 75%)", rgb: "rgb(192, 192, 192)"},
    {authored: "skyblue", name: "skyblue", hex: "#87ceeb", hsl: "hsl(197, 71%, 73%)", rgb: "rgb(135, 206, 235)"},
    {authored: "slateblue", name: "slateblue", hex: "#6a5acd", hsl: "hsl(248, 53%, 58%)", rgb: "rgb(106, 90, 205)"},
    {authored: "slategray", name: "slategray", hex: "#708090", hsl: "hsl(210, 13%, 50%)", rgb: "rgb(112, 128, 144)"},
    {authored: "slategrey", name: "slategray", hex: "#708090", hsl: "hsl(210, 13%, 50%)", rgb: "rgb(112, 128, 144)"},
    {authored: "snow", name: "snow", hex: "#fffafa", hsl: "hsl(0, 100%, 99%)", rgb: "rgb(255, 250, 250)"},
    {authored: "springgreen", name: "springgreen", hex: "#00ff7f", hsl: "hsl(150, 100%, 50%)", rgb: "rgb(0, 255, 127)"},
    {authored: "steelblue", name: "steelblue", hex: "#4682b4", hsl: "hsl(207, 44%, 49%)", rgb: "rgb(70, 130, 180)"},
    {authored: "tan", name: "tan", hex: "#d2b48c", hsl: "hsl(34, 44%, 69%)", rgb: "rgb(210, 180, 140)"},
    {authored: "teal", name: "teal", hex: "#008080", hsl: "hsl(180, 100%, 25%)", rgb: "rgb(0, 128, 128)"},
    {authored: "thistle", name: "thistle", hex: "#d8bfd8", hsl: "hsl(300, 24%, 80%)", rgb: "rgb(216, 191, 216)"},
    {authored: "tomato", name: "tomato", hex: "#ff6347", hsl: "hsl(9, 100%, 64%)", rgb: "rgb(255, 99, 71)"},
    {authored: "turquoise", name: "turquoise", hex: "#40e0d0", hsl: "hsl(174, 72%, 56%)", rgb: "rgb(64, 224, 208)"},
    {authored: "violet", name: "violet", hex: "#ee82ee", hsl: "hsl(300, 76%, 72%)", rgb: "rgb(238, 130, 238)"},
    {authored: "wheat", name: "wheat", hex: "#f5deb3", hsl: "hsl(39, 77%, 83%)", rgb: "rgb(245, 222, 179)"},
    {authored: "white", name: "white", hex: "#fff", hsl: "hsl(0, 0%, 100%)", rgb: "rgb(255, 255, 255)"},
    {authored: "whitesmoke", name: "whitesmoke", hex: "#f5f5f5", hsl: "hsl(0, 0%, 96%)", rgb: "rgb(245, 245, 245)"},
    {authored: "yellow", name: "yellow", hex: "#ff0", hsl: "hsl(60, 100%, 50%)", rgb: "rgb(255, 255, 0)"},
    {authored: "yellowgreen", name: "yellowgreen", hex: "#9acd32", hsl: "hsl(80, 61%, 50%)", rgb: "rgb(154, 205, 50)"},
    {authored: "rgba(0, 0, 0, 0)", name: "#0000", hex: "#0000", hsl: "hsla(0, 0%, 0%, 0)", rgb: "rgba(0, 0, 0, 0)"},
    {authored: "hsla(0, 0%, 0%, 0)", name: "#0000", hex: "#0000", hsl: "hsla(0, 0%, 0%, 0)", rgb: "rgba(0, 0, 0, 0)"},
    {authored: "rgba(50, 60, 70, 0.5)", name: "#323c4680", hex: "#323c4680", hsl: "hsla(210, 17%, 24%, 0.5)", rgb: "rgba(50, 60, 70, 0.5)"},
    {authored: "rgba(0, 0, 0, 0.3)", name: "#0000004d", hex: "#0000004d", hsl: "hsla(0, 0%, 0%, 0.3)", rgb: "rgba(0, 0, 0, 0.3)"},
    {authored: "rgba(255, 255, 255, 0.6)", name: "#fff9", hex: "#fff9", hsl: "hsla(0, 0%, 100%, 0.6)", rgb: "rgba(255, 255, 255, 0.6)"},
    {authored: "rgba(127, 89, 45, 1)", name: "#7f592d", hex: "#7f592d", hsl: "hsl(32, 48%, 34%)", rgb: "rgb(127, 89, 45)"},
    {authored: "hsla(19.304, 56%, 40%, 1)", name: "#9f512c", hex: "#9f512c", hsl: "hsl(19, 57%, 40%)", rgb: "rgb(159, 81, 44)"},
    {authored: "#f089", name: "#f089", hex: "#f089", hsl: "hsla(328, 100%, 50%, 0.6)", rgb: "rgba(255, 0, 136, 0.6)"},
    {authored: "#00ff8080", name: "#00ff8080", hex: "#00ff8080", hsl: "hsla(150, 100%, 50%, 0.5)", rgb: "rgba(0, 255, 128, 0.5)"},
    {authored: "currentcolor", name: "currentcolor", hex: "currentcolor", hsl: "currentcolor", rgb: "currentcolor"},
    {authored: "inherit", name: "inherit", hex: "inherit", hsl: "inherit", rgb: "inherit"},
    {authored: "initial", name: "initial", hex: "initial", hsl: "initial", rgb: "initial"},
    {authored: "invalidColor", name: "", hex: "", hsl: "", rgb: ""},
    {authored: "transparent", name: "transparent", hex: "transparent", hsl: "transparent", rgb: "transparent"},
    {authored: "unset", name: "unset", hex: "unset", hsl: "unset", rgb: "unset"}
  ];
}
