/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const OutputParser = require("devtools/client/shared/output-parser");
const {initCssProperties, getCssProperties} = require("devtools/shared/fronts/css-properties");

add_task(function* () {
  yield addTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, , doc] = yield createHost("bottom", "data:text/html," +
    "<h1>browser_outputParser.js</h1><div></div>");

  // Mock the toolbox that initCssProperties expect so we get the fallback css properties.
  let toolbox = {target: {client: {}, hasActor: () => false}};
  yield initCssProperties(toolbox);
  let cssProperties = getCssProperties(toolbox);

  let parser = new OutputParser(doc, cssProperties);
  testParseCssProperty(doc, parser);
  testParseCssVar(doc, parser);
  testParseURL(doc, parser);
  testParseFilter(doc, parser);
  testParseAngle(doc, parser);

  host.destroy();
}

// Class name used in color swatch.
var COLOR_TEST_CLASS = "test-class";

// Create a new CSS color-parsing test.  |name| is the name of the CSS
// property.  |value| is the CSS text to use.  |segments| is an array
// describing the expected result.  If an element of |segments| is a
// string, it is simply appended to the expected string.  Otherwise,
// it must be an object with a |name| property, which is the color
// name as it appears in the input.
//
// This approach is taken to reduce boilerplate and to make it simpler
// to modify the test when the parseCssProperty output changes.
function makeColorTest(name, value, segments) {
  let result = {
    name,
    value,
    expected: ""
  };

  for (let segment of segments) {
    if (typeof (segment) === "string") {
      result.expected += segment;
    } else {
      result.expected += "<span data-color=\"" + segment.name + "\">" +
        "<span class=\"" + COLOR_TEST_CLASS + "\" style=\"background-color:" +
        segment.name + "\"></span><span>" +
        segment.name + "</span></span>";
    }
  }

  result.desc = "Testing " + name + ": " + value;

  return result;
}

function testParseCssProperty(doc, parser) {
  let tests = [
    makeColorTest("border", "1px solid red",
                  ["1px solid ", {name: "red"}]),

    makeColorTest("background-image",
      "linear-gradient(to right, #F60 10%, rgba(0,0,0,1))",
      ["linear-gradient(to right, ", {name: "#F60"},
       " 10%, ", {name: "rgba(0,0,0,1)"},
       ")"]),

    // In "arial black", "black" is a font, not a color.
    makeColorTest("font-family", "arial black", ["arial black"]),

    makeColorTest("box-shadow", "0 0 1em red",
                  ["0 0 1em ", {name: "red"}]),

    makeColorTest("box-shadow",
      "0 0 1em red, 2px 2px 0 0 rgba(0,0,0,.5)",
      ["0 0 1em ", {name: "red"},
       ", 2px 2px 0 0 ",
      {name: "rgba(0,0,0,.5)"}]),

    makeColorTest("content", "\"red\"", ["\"red\""]),

    // Invalid property names should not cause exceptions.
    makeColorTest("hellothere", "'red'", ["'red'"]),

    makeColorTest("filter",
      "blur(1px) drop-shadow(0 0 0 blue) url(red.svg#blue)",
      ["<span data-filters=\"blur(1px) drop-shadow(0 0 0 blue) ",
       "url(red.svg#blue)\"><span>",
       "blur(1px) drop-shadow(0 0 0 ",
       {name: "blue"},
       ") url(red.svg#blue)</span></span>"]),

    makeColorTest("color", "currentColor", ["currentColor"]),

    // Test a very long property.
    makeColorTest("background-image",
      /* eslint-disable max-len */
      "linear-gradient(to left, transparent 0, transparent 5%,#F00 0, #F00 10%,#FF0 0, #FF0 15%,#0F0 0, #0F0 20%,#0FF 0, #0FF 25%,#00F 0, #00F 30%,#800 0, #800 35%,#880 0, #880 40%,#080 0, #080 45%,#088 0, #088 50%,#008 0, #008 55%,#FFF 0, #FFF 60%,#EEE 0, #EEE 65%,#CCC 0, #CCC 70%,#999 0, #999 75%,#666 0, #666 80%,#333 0, #333 85%,#111 0, #111 90%,#000 0, #000 95%,transparent 0, transparent 100%)",
      /* eslint-enable max-len */
      ["linear-gradient(to left, ", {name: "transparent"},
       " 0, ", {name: "transparent"},
       " 5%,", {name: "#F00"},
       " 0, ", {name: "#F00"},
       " 10%,", {name: "#FF0"},
       " 0, ", {name: "#FF0"},
       " 15%,", {name: "#0F0"},
       " 0, ", {name: "#0F0"},
       " 20%,", {name: "#0FF"},
       " 0, ", {name: "#0FF"},
       " 25%,", {name: "#00F"},
       " 0, ", {name: "#00F"},
       " 30%,", {name: "#800"},
       " 0, ", {name: "#800"},
       " 35%,", {name: "#880"},
       " 0, ", {name: "#880"},
       " 40%,", {name: "#080"},
       " 0, ", {name: "#080"},
       " 45%,", {name: "#088"},
       " 0, ", {name: "#088"},
       " 50%,", {name: "#008"},
       " 0, ", {name: "#008"},
       " 55%,", {name: "#FFF"},
       " 0, ", {name: "#FFF"},
       " 60%,", {name: "#EEE"},
       " 0, ", {name: "#EEE"},
       " 65%,", {name: "#CCC"},
       " 0, ", {name: "#CCC"},
       " 70%,", {name: "#999"},
       " 0, ", {name: "#999"},
       " 75%,", {name: "#666"},
       " 0, ", {name: "#666"},
       " 80%,", {name: "#333"},
       " 0, ", {name: "#333"},
       " 85%,", {name: "#111"},
       " 0, ", {name: "#111"},
       " 90%,", {name: "#000"},
       " 0, ", {name: "#000"},
       " 95%,", {name: "transparent"},
       " 0, ", {name: "transparent"},
       " 100%)"]),
  ];

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");

  for (let test of tests) {
    info(test.desc);

    let frag = parser.parseCssProperty(test.name, test.value, {
      colorSwatchClass: COLOR_TEST_CLASS
    });

    target.appendChild(frag);

    is(target.innerHTML, test.expected,
       "CSS property correctly parsed for " + test.name + ": " + test.value);

    target.innerHTML = "";
  }
}

function testParseCssVar(doc, parser) {
  let frag = parser.parseCssProperty("color", "var(--some-kind-of-green)", {
    colorSwatchClass: "test-colorswatch"
  });

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  is(target.innerHTML, "var(--some-kind-of-green)",
     "CSS property correctly parsed");

  target.innerHTML = "";
}

function testParseURL(doc, parser) {
  info("Test that URL parsing preserves quoting style");

  const tests = [
    {
      desc: "simple test without quotes",
      leader: "url(",
      trailer: ")",
    },
    {
      desc: "simple test with single quotes",
      leader: "url('",
      trailer: "')",
    },
    {
      desc: "simple test with double quotes",
      leader: "url(\"",
      trailer: "\")",
    },
    {
      desc: "test with single quotes and whitespace",
      leader: "url( \t'",
      trailer: "'\r\n\f)",
    },
    {
      desc: "simple test with uppercase",
      leader: "URL(",
      trailer: ")",
    },
    {
      desc: "bad url, missing paren",
      leader: "url(",
      trailer: "",
      expectedTrailer: ")"
    },
    {
      desc: "bad url, missing paren, with baseURI",
      baseURI: "data:text/html,<style></style>",
      leader: "url(",
      trailer: "",
      expectedTrailer: ")"
    },
    {
      desc: "bad url, double quote, missing paren",
      leader: "url(\"",
      trailer: "\"",
      expectedTrailer: "\")",
    },
    {
      desc: "bad url, single quote, missing paren and quote",
      leader: "url('",
      trailer: "",
      expectedTrailer: "')"
    }
  ];

  for (let test of tests) {
    let url = test.leader + "something.jpg" + test.trailer;
    let frag = parser.parseCssProperty("background", url, {
      urlClass: "test-urlclass",
      baseURI: test.baseURI,
    });

    let target = doc.querySelector("div");
    target.appendChild(frag);

    let expectedTrailer = test.expectedTrailer || test.trailer;

    let expected = test.leader +
        "<a target=\"_blank\" class=\"test-urlclass\" " +
        "href=\"something.jpg\">something.jpg</a>" +
        expectedTrailer;

    is(target.innerHTML, expected, test.desc);

    target.innerHTML = "";
  }
}

function testParseFilter(doc, parser) {
  let frag = parser.parseCssProperty("filter", "something invalid", {
    filterSwatchClass: "test-filterswatch"
  });

  let swatchCount = frag.querySelectorAll(".test-filterswatch").length;
  is(swatchCount, 1, "filter swatch was created");
}

function testParseAngle(doc, parser) {
  let frag = parser.parseCssProperty("image-orientation", "90deg", {
    angleSwatchClass: "test-angleswatch"
  });

  let swatchCount = frag.querySelectorAll(".test-angleswatch").length;
  is(swatchCount, 1, "angle swatch was created");

  frag = parser.parseCssProperty("background-image",
    "linear-gradient(90deg, red, blue", {
      angleSwatchClass: "test-angleswatch"
    });

  swatchCount = frag.querySelectorAll(".test-angleswatch").length;
  is(swatchCount, 1, "angle swatch was created");
}
