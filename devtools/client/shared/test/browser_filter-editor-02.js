/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the Filter Editor Widget renders filters correctly

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/client/locales/filterwidget.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

const TEST_URI = CHROME_URL_ROOT + "doc_filter-editor-01.html";

add_task(async function() {
  const [,, doc] = await createHost("bottom", TEST_URI);
  const cssIsValid = getClientCssProperties().getValidityChecker(doc);

  const TEST_DATA = [
    {
      cssValue: "blur(2px) contrast(200%) hue-rotate(20.2deg) drop-shadow(5px 5px black)",
      expected: [
        {
          label: "blur",
          value: "2",
          unit: "px"
        },
        {
          label: "contrast",
          value: "200",
          unit: "%"
        },
        {
          label: "hue-rotate",
          value: "20.2",
          unit: "deg"
        },
        {
          label: "drop-shadow",
          value: "5px 5px black",
          unit: null
        }
      ]
    },
    {
      cssValue: "hue-rotate(420.2deg)",
      expected: [
        {
          label: "hue-rotate",
          value: "420.2",
          unit: "deg"
        }
      ]
    },
    {
      cssValue: "url(example.svg)",
      expected: [
        {
          label: "url",
          value: "example.svg",
          unit: null
        }
      ]
    },
    {
      cssValue: "none",
      expected: []
    }
  ];

  const container = doc.querySelector("#filter-container");
  const widget = new CSSFilterEditorWidget(container, "none", cssIsValid);

  info("Test rendering of different types");

  for (const {cssValue, expected} of TEST_DATA) {
    widget.setCssValue(cssValue);

    if (cssValue === "none") {
      const text = container.querySelector("#filters").textContent;
      ok(text.indexOf(L10N.getStr("emptyFilterList")) > -1,
         "Contains |emptyFilterList| string when given value 'none'");
      ok(text.indexOf(L10N.getStr("addUsingList")) > -1,
         "Contains |addUsingList| string when given value 'none'");
      continue;
    }
    const filters = container.querySelectorAll(".filter");
    testRenderedFilters(filters, expected);
  }
});

function testRenderedFilters(filters, expected) {
  for (const [index, filter] of [...filters].entries()) {
    const [name, value] = filter.children,
      label = name.children[1],
      [input, unit] = value.children;

    const eq = expected[index];
    is(label.textContent, eq.label, "Label should match");
    is(input.value, eq.value, "Values should match");
    if (eq.unit) {
      is(unit.textContent, eq.unit, "Unit should match");
    }
  }
}
