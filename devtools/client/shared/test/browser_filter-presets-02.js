/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests loading presets

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

const TEST_URI = CHROME_URL_ROOT + "doc_filter-editor-01.html";

add_task(async function() {
  const [,, doc] = await createHost("bottom", TEST_URI);
  const cssIsValid = getClientCssProperties().getValidityChecker(doc);

  const container = doc.querySelector("#filter-container");
  const widget = new CSSFilterEditorWidget(container, "none", cssIsValid);
  // First render
  await widget.once("render");

  const VALUE = "blur(2px) contrast(150%)";
  const NAME = "Test";

  await showFilterPopupPresetsAndCreatePreset(widget, NAME, VALUE);

  let onRender = widget.once("render");
  // reset value
  widget.setCssValue("saturate(100%) brightness(150%)");
  await onRender;

  const preset = widget.el.querySelector(".preset");

  onRender = widget.once("render");
  widget._presetClick({
    target: preset
  });

  await onRender;

  is(widget.getCssValue(), VALUE,
     "Should set widget's value correctly");
  is(widget.el.querySelector(".presets-list .footer input").value, NAME,
     "Should set input's value to name");
});
