/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the Filter Editor Widget renders filters correctly

const TEST_URI = "chrome://devtools/content/shared/widgets/filter-frame.xhtml";
const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");

const { ViewHelpers } = Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm", {});
const STRINGS_URI = "chrome://browser/locale/devtools/filterwidget.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

add_task(function*() {
  yield promiseTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

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

  const container = doc.querySelector("#container");
  let widget = new CSSFilterEditorWidget(container, "none");

  info("Test rendering of different types");


  for (let {cssValue, expected} of TEST_DATA) {
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
  for (let [index, filter] of [...filters].entries()) {
    let [name, value] = filter.children,
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
