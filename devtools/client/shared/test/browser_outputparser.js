/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
var {Loader} = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js",
                         {});
var {OutputParser} = require("devtools/shared/output-parser");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, , doc] = yield createHost("bottom", "data:text/html," +
    "<h1>browser_outputParser.js</h1><div></div>");

  let parser = new OutputParser(doc);
  testParseCssProperty(doc, parser);
  testParseCssVar(doc, parser);
  testParseURL(doc, parser);

  host.destroy();
}

// Class name used in color swatch.
var COLOR_TEST_CLASS = "test-class";

// Create a new CSS color-parsing test.  |name| is the name of the CSS
// property.  |value| is the CSS text to use.  |segments| is an array
// describing the expected result.  If an element of |segments| is a
// string, it is simply appended to the expected string.  Otherwise,
// it must be an object with a |value| property, which is the color
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
        "<span style=\"background-color:" + segment.name +
        "\" class=\"" + COLOR_TEST_CLASS + "\"></span><span>" +
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
      urlClass: "test-urlclass"
    });

    let target = doc.querySelector("div");
    target.appendChild(frag);

    let expectedTrailer = test.expectedTrailer || test.trailer;

    let expected = test.leader +
        "<a href=\"something.jpg\" class=\"test-urlclass\" " +
        "target=\"_blank\">something.jpg</a>" +
        expectedTrailer;

    is(target.innerHTML, expected, test.desc);

    target.innerHTML = "";
  }
}
