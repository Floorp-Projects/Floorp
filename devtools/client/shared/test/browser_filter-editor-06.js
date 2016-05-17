/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Widget's add button

const TEST_URI = "chrome://devtools/content/shared/widgets/filter-frame.xhtml";

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");

const { LocalizationHelper } = require("devtools/client/shared/l10n");
const STRINGS_URI = "chrome://devtools/locale/filterwidget.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

add_task(function* () {
  yield addTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

  const container = doc.querySelector("#container");
  let widget = new CSSFilterEditorWidget(container, "none");

  const select = widget.el.querySelector("select"),
    add = widget.el.querySelector("#add-filter");

  const TEST_DATA = [
    {
      name: "blur",
      unit: "px",
      type: "length"
    },
    {
      name: "contrast",
      unit: "%",
      type: "percentage"
    },
    {
      name: "hue-rotate",
      unit: "deg",
      type: "angle"
    },
    {
      name: "drop-shadow",
      placeholder: L10N.getStr("dropShadowPlaceholder"),
      type: "string"
    },
    {
      name: "url",
      placeholder: "example.svg#c1",
      type: "string"
    }
  ];

  info("Test adding new filters with different units");

  for (let [index, filter] of TEST_DATA.entries()) {
    select.value = filter.name;
    add.click();

    if (filter.unit) {
      is(widget.getValueAt(index), `0${filter.unit}`,
         `Should add ${filter.unit} to ${filter.type} filters`);
    } else if (filter.placeholder) {
      let i = index + 1;
      const input = widget.el.querySelector(`.filter:nth-child(${i}) input`);
      is(input.placeholder, filter.placeholder,
         "Should set the appropriate placeholder for string-type filters");
    }
  }
});
