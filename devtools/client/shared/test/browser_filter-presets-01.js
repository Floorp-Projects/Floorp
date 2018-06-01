/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests saving presets

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

  const preset = widget.el.querySelector(".preset");
  is(preset.querySelector("label").textContent, NAME,
     "Should show preset name correctly");
  is(preset.querySelector("span").textContent, VALUE,
     "Should show preset value preview correctly");

  let list = await widget.getPresets();
  const input = widget.el.querySelector(".presets-list .footer input");
  let data = list[0];

  is(data.name, NAME,
     "Should add the preset to asyncStorage - name property");
  is(data.value, VALUE,
     "Should add the preset to asyncStorage - name property");

  info("Test overriding preset by using the same name");

  const VALUE_2 = "saturate(50%) brightness(10%)";

  widget.setCssValue(VALUE_2);

  await savePreset(widget);

  is(widget.el.querySelectorAll(".preset").length, 1,
     "Should override the preset with the same name - render");

  list = await widget.getPresets();
  data = list[0];

  is(list.length, 1,
     "Should override the preset with the same name - asyncStorage");

  is(data.name, NAME,
     "Should override the preset with the same name - prop name");
  is(data.value, VALUE_2,
     "Should override the preset with the same name - prop value");

  await widget.setPresets([]);

  info("Test saving a preset without name");
  input.value = "";

  await savePreset(widget, "preset-save-error");

  list = await widget.getPresets();
  is(list.length, 0,
     "Should not add a preset without name");

  info("Test saving a preset without filters");

  input.value = NAME;
  widget.setCssValue("none");

  await savePreset(widget, "preset-save-error");

  list = await widget.getPresets();
  is(list.length, 0,
     "Should not add a preset without filters (value: none)");
});

/**
 * Call savePreset on widget and wait for the specified event to emit
 * @param {CSSFilterWidget} widget
 * @param {string} expectEvent="render" The event to listen on
 * @return {Promise}
 */
function savePreset(widget, expectEvent = "render") {
  const onEvent = widget.once(expectEvent);
  widget._savePreset({
    preventDefault: () => {},
  });
  return onEvent;
}
