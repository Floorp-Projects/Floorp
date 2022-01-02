/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  getClientCssProperties,
} = require("devtools/client/fronts/css-properties");

add_task(async function() {
  await pushPref("layout.css.backdrop-filter.enabled", true);
  await pushPref("layout.css.individual-transform.enabled", true);
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
});

async function performTest() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.allow_unsafe_parent_loads", true]],
  });

  const OutputParser = require("devtools/client/shared/output-parser");

  const { host, doc } = await createHost(
    "bottom",
    "data:text/html," + "<h1>browser_outputParser.js</h1><div></div>"
  );

  const cssProperties = getClientCssProperties();

  const parser = new OutputParser(doc, cssProperties);
  testParseCssProperty(doc, parser);
  testParseCssVar(doc, parser);
  testParseURL(doc, parser);
  testParseFilter(doc, parser);
  testParseBackdropFilter(doc, parser);
  testParseAngle(doc, parser);
  testParseShape(doc, parser);
  testParseVariable(doc, parser);
  testParseColorVariable(doc, parser);
  testParseFontFamily(doc, parser);

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
  const result = {
    name,
    value,
    expected: "",
  };

  for (const segment of segments) {
    if (typeof segment === "string") {
      result.expected += segment;
    } else {
      result.expected +=
        '<span data-color="' +
        segment.name +
        '">' +
        '<span class="' +
        COLOR_TEST_CLASS +
        '" style="background-color:' +
        segment.name +
        '" tabindex="0" role="button"></span><span>' +
        segment.name +
        "</span></span>";
    }
  }

  result.desc = "Testing " + name + ": " + value;

  return result;
}

function testParseCssProperty(doc, parser) {
  const tests = [
    makeColorTest("border", "1px solid red", ["1px solid ", { name: "red" }]),

    makeColorTest(
      "background-image",
      "linear-gradient(to right, #F60 10%, rgba(0,0,0,1))",
      [
        "linear-gradient(to right, ",
        { name: "#F60" },
        " 10%, ",
        { name: "rgba(0,0,0,1)" },
        ")",
      ]
    ),

    // In "arial black", "black" is a font, not a color.
    // (The font-family parser creates a span)
    makeColorTest("font-family", "arial black", ["<span>arial black</span>"]),

    makeColorTest("box-shadow", "0 0 1em red", ["0 0 1em ", { name: "red" }]),

    makeColorTest("box-shadow", "0 0 1em red, 2px 2px 0 0 rgba(0,0,0,.5)", [
      "0 0 1em ",
      { name: "red" },
      ", 2px 2px 0 0 ",
      { name: "rgba(0,0,0,.5)" },
    ]),

    makeColorTest("content", '"red"', ['"red"']),

    // Invalid property names should not cause exceptions.
    makeColorTest("hellothere", "'red'", ["'red'"]),

    makeColorTest(
      "filter",
      "blur(1px) drop-shadow(0 0 0 blue) url(red.svg#blue)",
      [
        '<span data-filters="blur(1px) drop-shadow(0 0 0 blue) ',
        'url(red.svg#blue)"><span>',
        "blur(1px) drop-shadow(0 0 0 ",
        { name: "blue" },
        ") url(red.svg#blue)</span></span>",
      ]
    ),

    makeColorTest("color", "currentColor", ["currentColor"]),

    // Test a very long property.
    makeColorTest(
      "background-image",
      /* eslint-disable max-len */
      "linear-gradient(to left, transparent 0, transparent 5%,#F00 0, #F00 10%,#FF0 0, #FF0 15%,#0F0 0, #0F0 20%,#0FF 0, #0FF 25%,#00F 0, #00F 30%,#800 0, #800 35%,#880 0, #880 40%,#080 0, #080 45%,#088 0, #088 50%,#008 0, #008 55%,#FFF 0, #FFF 60%,#EEE 0, #EEE 65%,#CCC 0, #CCC 70%,#999 0, #999 75%,#666 0, #666 80%,#333 0, #333 85%,#111 0, #111 90%,#000 0, #000 95%,transparent 0, transparent 100%)",
      /* eslint-enable max-len */
      [
        "linear-gradient(to left, ",
        { name: "transparent" },
        " 0, ",
        { name: "transparent" },
        " 5%,",
        { name: "#F00" },
        " 0, ",
        { name: "#F00" },
        " 10%,",
        { name: "#FF0" },
        " 0, ",
        { name: "#FF0" },
        " 15%,",
        { name: "#0F0" },
        " 0, ",
        { name: "#0F0" },
        " 20%,",
        { name: "#0FF" },
        " 0, ",
        { name: "#0FF" },
        " 25%,",
        { name: "#00F" },
        " 0, ",
        { name: "#00F" },
        " 30%,",
        { name: "#800" },
        " 0, ",
        { name: "#800" },
        " 35%,",
        { name: "#880" },
        " 0, ",
        { name: "#880" },
        " 40%,",
        { name: "#080" },
        " 0, ",
        { name: "#080" },
        " 45%,",
        { name: "#088" },
        " 0, ",
        { name: "#088" },
        " 50%,",
        { name: "#008" },
        " 0, ",
        { name: "#008" },
        " 55%,",
        { name: "#FFF" },
        " 0, ",
        { name: "#FFF" },
        " 60%,",
        { name: "#EEE" },
        " 0, ",
        { name: "#EEE" },
        " 65%,",
        { name: "#CCC" },
        " 0, ",
        { name: "#CCC" },
        " 70%,",
        { name: "#999" },
        " 0, ",
        { name: "#999" },
        " 75%,",
        { name: "#666" },
        " 0, ",
        { name: "#666" },
        " 80%,",
        { name: "#333" },
        " 0, ",
        { name: "#333" },
        " 85%,",
        { name: "#111" },
        " 0, ",
        { name: "#111" },
        " 90%,",
        { name: "#000" },
        " 0, ",
        { name: "#000" },
        " 95%,",
        { name: "transparent" },
        " 0, ",
        { name: "transparent" },
        " 100%)",
      ]
    ),

    // Note the lack of a space before the color here.
    makeColorTest("border", "1px dotted#f06", [
      "1px dotted ",
      { name: "#f06" },
    ]),
  ];

  const target = doc.querySelector("div");
  ok(target, "captain, we have the div");

  for (const test of tests) {
    info(test.desc);

    const frag = parser.parseCssProperty(test.name, test.value, {
      colorSwatchClass: COLOR_TEST_CLASS,
    });

    target.appendChild(frag);

    is(
      target.innerHTML,
      test.expected,
      "CSS property correctly parsed for " + test.name + ": " + test.value
    );

    target.innerHTML = "";
  }
}

function testParseCssVar(doc, parser) {
  const frag = parser.parseCssProperty("color", "var(--some-kind-of-green)", {
    colorSwatchClass: "test-colorswatch",
  });

  const target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  is(
    target.innerHTML,
    "var(--some-kind-of-green)",
    "CSS property correctly parsed"
  );

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
      leader: 'url("',
      trailer: '")',
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
      expectedTrailer: ")",
    },
    {
      desc: "bad url, missing paren, with baseURI",
      baseURI: "data:text/html,<style></style>",
      leader: "url(",
      trailer: "",
      expectedTrailer: ")",
    },
    {
      desc: "bad url, double quote, missing paren",
      leader: 'url("',
      trailer: '"',
      expectedTrailer: '")',
    },
    {
      desc: "bad url, single quote, missing paren and quote",
      leader: "url('",
      trailer: "",
      expectedTrailer: "')",
    },
  ];

  for (const test of tests) {
    const url = test.leader + "something.jpg" + test.trailer;
    const frag = parser.parseCssProperty("background", url, {
      urlClass: "test-urlclass",
      baseURI: test.baseURI,
    });

    const target = doc.querySelector("div");
    target.appendChild(frag);

    const expectedTrailer = test.expectedTrailer || test.trailer;

    const expected =
      test.leader +
      '<a target="_blank" class="test-urlclass" ' +
      'href="something.jpg">something.jpg</a>' +
      expectedTrailer;

    is(target.innerHTML, expected, test.desc);

    target.innerHTML = "";
  }
}

function testParseFilter(doc, parser) {
  const frag = parser.parseCssProperty("filter", "something invalid", {
    filterSwatchClass: "test-filterswatch",
  });

  const swatchCount = frag.querySelectorAll(".test-filterswatch").length;
  is(swatchCount, 1, "filter swatch was created");
}

function testParseBackdropFilter(doc, parser) {
  const frag = parser.parseCssProperty("backdrop-filter", "something invalid", {
    filterSwatchClass: "test-filterswatch",
  });

  const swatchCount = frag.querySelectorAll(".test-filterswatch").length;
  is(swatchCount, 1, "filter swatch was created for backdrop-filter");
}

function testParseAngle(doc, parser) {
  let frag = parser.parseCssProperty("rotate", "90deg", {
    angleSwatchClass: "test-angleswatch",
  });

  let swatchCount = frag.querySelectorAll(".test-angleswatch").length;
  is(swatchCount, 1, "angle swatch was created");

  frag = parser.parseCssProperty(
    "background-image",
    "linear-gradient(90deg, red, blue",
    {
      angleSwatchClass: "test-angleswatch",
    }
  );

  swatchCount = frag.querySelectorAll(".test-angleswatch").length;
  is(swatchCount, 1, "angle swatch was created");
}

function testParseShape(doc, parser) {
  info("Test shape parsing");

  const tests = [
    {
      desc: "Polygon shape",
      definition:
        "polygon(evenodd, 0px 0px, 10%200px,30%30% , calc(250px - 10px) 0 ,\n " +
        "12em var(--variable), 100% 100%) margin-box",
      spanCount: 18,
    },
    {
      desc: "Invalid polygon shape",
      definition: "polygon(0px 0px 100px 20px, 20% 20%)",
      spanCount: 0,
    },
    {
      desc: "Circle shape with all arguments",
      definition: "circle(25% at\n 30% 200px) border-box",
      spanCount: 4,
    },
    {
      desc: "Circle shape with only one center",
      definition: "circle(25em at 40%)",
      spanCount: 3,
    },
    {
      desc: "Circle shape with no radius",
      definition: "circle(at 30% 40%)",
      spanCount: 3,
    },
    {
      desc: "Circle shape with no center",
      definition: "circle(12em)",
      spanCount: 1,
    },
    {
      desc: "Circle shape with no arguments",
      definition: "circle()",
      spanCount: 0,
    },
    {
      desc: "Circle shape with no space before at",
      definition: "circle(25%at 30% 30%)",
      spanCount: 4,
    },
    {
      desc: "Invalid circle shape",
      definition: "circle(25%at30%30%)",
      spanCount: 0,
    },
    {
      desc: "Ellipse shape with all arguments",
      definition: "ellipse(200px 10em at 25% 120px) content-box",
      spanCount: 5,
    },
    {
      desc: "Ellipse shape with only one center",
      definition: "ellipse(200px 10% at 120px)",
      spanCount: 4,
    },
    {
      desc: "Ellipse shape with no radius",
      definition: "ellipse(at 25% 120px)",
      spanCount: 3,
    },
    {
      desc: "Ellipse shape with no center",
      definition: "ellipse(200px\n10em)",
      spanCount: 2,
    },
    {
      desc: "Ellipse shape with no arguments",
      definition: "ellipse()",
      spanCount: 0,
    },
    {
      desc: "Invalid ellipse shape",
      definition: "ellipse(200px100px at 30$ 20%)",
      spanCount: 0,
    },
    {
      desc: "Inset shape with 4 arguments",
      definition: "inset(200px 100px\n 30%15%)",
      spanCount: 4,
    },
    {
      desc: "Inset shape with 3 arguments",
      definition: "inset(200px 100px 15%)",
      spanCount: 3,
    },
    {
      desc: "Inset shape with 2 arguments",
      definition: "inset(200px 100px)",
      spanCount: 2,
    },
    {
      desc: "Inset shape with 1 argument",
      definition: "inset(200px)",
      spanCount: 1,
    },
    {
      desc: "Inset shape with 0 arguments",
      definition: "inset()",
      spanCount: 0,
    },
  ];

  for (const { desc, definition, spanCount } of tests) {
    info(desc);
    const frag = parser.parseCssProperty("clip-path", definition, {
      shapeClass: "ruleview-shape",
    });
    const spans = frag.querySelectorAll(".ruleview-shape-point");
    is(spans.length, spanCount, desc + " span count");
    is(frag.textContent, definition, desc + " text content");
  }
}

function testParseVariable(doc, parser) {
  const TESTS = [
    {
      text: "var(--seen)",
      variables: { "--seen": "chartreuse" },
      expected:
        /* eslint-disable */
        '<span data-color="chartreuse">' +
          "<span>var(" +
            '<span data-variable="--seen = chartreuse">--seen</span>)' +
          "</span>" +
        "</span>",
        /* eslint-enable */
    },
    {
      text: "var(--not-seen)",
      variables: {},
      expected:
        /* eslint-disable */
        "<span>var(" +
          '<span class="unmatched-class" data-variable="--not-seen is not set">--not-seen</span>' +
        ")</span>",
        /* eslint-enable */
    },
    {
      text: "var(--seen, seagreen)",
      variables: { "--seen": "chartreuse" },
      expected:
        /* eslint-disable */
        '<span data-color="chartreuse">' +
          "<span>var(" +
            '<span data-variable="--seen = chartreuse">--seen</span>,' +
            '<span class="unmatched-class"> ' +
              '<span data-color="seagreen">' +
                "<span>seagreen</span>" +
              "</span>" +
            "</span>)" +
          "</span>" +
        "</span>",
        /* eslint-enable */
    },
    {
      text: "var(--not-seen, var(--seen))",
      variables: { "--seen": "chartreuse" },
      expected:
        /* eslint-disable */
        "<span>var(" +
          '<span class="unmatched-class" data-variable="--not-seen is not set">--not-seen</span>,' +
          "<span> " +
            '<span data-color="chartreuse">' +
              "<span>var(" +
                '<span data-variable="--seen = chartreuse">--seen</span>)' +
              "</span>" +
            "</span>" +
          "</span>)" +
        "</span>",
        /* eslint-enable */
    },
  ];

  for (const test of TESTS) {
    const getValue = function(varName) {
      return test.variables[varName];
    };

    const frag = parser.parseCssProperty("color", test.text, {
      getVariableValue: getValue,
      unmatchedVariableClass: "unmatched-class",
    });

    const target = doc.querySelector("div");
    target.appendChild(frag);

    is(target.innerHTML, test.expected, test.text);
    target.innerHTML = "";
  }
}

function testParseColorVariable(doc, parser) {
  const testCategories = [
    {
      desc: "Test for CSS variable defining color",
      tests: [
        makeColorTest("--test-var", "lime", [{ name: "lime" }]),
        makeColorTest("--test-var", "#000", [{ name: "#000" }]),
      ],
    },
    {
      desc: "Test for CSS variable not defining color",
      tests: [
        makeColorTest("--foo", "something", ["something"]),
        makeColorTest("--bar", "Arial Black", ["Arial Black"]),
        makeColorTest("--baz", "10vmin", ["10vmin"]),
      ],
    },
    {
      desc: "Test for non CSS variable defining color",
      tests: [
        makeColorTest("non-css-variable", "lime", ["lime"]),
        makeColorTest("-non-css-variable", "#000", ["#000"]),
      ],
    },
  ];

  for (const category of testCategories) {
    info(category.desc);

    for (const test of category.tests) {
      info(test.desc);
      const target = doc.querySelector("div");

      const frag = parser.parseCssProperty(test.name, test.value, {
        colorSwatchClass: COLOR_TEST_CLASS,
      });

      target.appendChild(frag);

      is(
        target.innerHTML,
        test.expected,
        `The parsed result for '${test.name}: ${test.value}' is correct`
      );

      target.innerHTML = "";
    }
  }
}

function testParseFontFamily(doc, parser) {
  info("Test font-family parsing");
  const tests = [
    {
      desc: "No fonts",
      definition: "",
      families: [],
    },
    {
      desc: "List of fonts",
      definition: "Arial,Helvetica,sans-serif",
      families: ["Arial", "Helvetica", "sans-serif"],
    },
    {
      desc: "Fonts with spaces",
      definition: "Open Sans",
      families: ["Open Sans"],
    },
    {
      desc: "Quoted fonts",
      definition: "\"Arial\",'Open Sans'",
      families: ["Arial", "Open Sans"],
    },
    {
      desc: "Fonts with extra whitespace",
      definition: " Open  Sans  ",
      families: ["Open Sans"],
    },
  ];

  const textContentTests = [
    {
      desc: "No whitespace between fonts",
      definition: "Arial,Helvetica,sans-serif",
      output: "Arial,Helvetica,sans-serif",
    },
    {
      desc: "Whitespace between fonts",
      definition: "Arial ,  Helvetica,   sans-serif",
      output: "Arial , Helvetica, sans-serif",
    },
    {
      desc: "Whitespace before first font trimmed",
      definition: "  Arial,Helvetica,sans-serif",
      output: "Arial,Helvetica,sans-serif",
    },
    {
      desc: "Whitespace after last font trimmed",
      definition: "Arial,Helvetica,sans-serif  ",
      output: "Arial,Helvetica,sans-serif",
    },
    {
      desc: "Whitespace between quoted fonts",
      definition: "'Arial' ,  \"Helvetica\" ",
      output: "'Arial' , \"Helvetica\"",
    },
    {
      desc: "Whitespace within font preserved",
      definition: "'  Ari al '",
      output: "'  Ari al '",
    },
  ];

  for (const { desc, definition, families } of tests) {
    info(desc);
    const frag = parser.parseCssProperty("font-family", definition, {
      fontFamilyClass: "ruleview-font-family",
    });
    const spans = frag.querySelectorAll(".ruleview-font-family");

    is(spans.length, families.length, desc + " span count");
    for (let i = 0; i < spans.length; i++) {
      is(spans[i].textContent, families[i], desc + " span contents");
    }
  }

  info("Test font-family text content");
  for (const { desc, definition, output } of textContentTests) {
    info(desc);
    const frag = parser.parseCssProperty("font-family", definition, {});
    is(frag.textContent, output, desc + " text content matches");
  }
}
