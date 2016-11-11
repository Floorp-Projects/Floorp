/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests loading presets

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

const TEST_URI = `data:text/html,<div id="filter-container" />`;

add_task(function* () {
  let [,, doc] = yield createHost("bottom", TEST_URI);
  const cssIsValid = getClientCssProperties().getValidityChecker(doc);

  const container = doc.querySelector("#filter-container");
  let widget = new CSSFilterEditorWidget(container, "none", cssIsValid);
  // First render
  yield widget.once("render");

  const VALUE = "blur(2px) contrast(150%)";
  const NAME = "Test";

  yield showFilterPopupPresetsAndCreatePreset(widget, NAME, VALUE);

  let onRender = widget.once("render");
  // reset value
  widget.setCssValue("saturate(100%) brightness(150%)");
  yield onRender;

  let preset = widget.el.querySelector(".preset");

  onRender = widget.once("render");
  widget._presetClick({
    target: preset
  });

  yield onRender;

  is(widget.getCssValue(), VALUE,
     "Should set widget's value correctly");
  is(widget.el.querySelector(".presets-list .footer input").value, NAME,
     "Should set input's value to name");
});
