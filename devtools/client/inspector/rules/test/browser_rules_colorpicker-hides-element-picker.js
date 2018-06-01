/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that on selecting colorpicker eyedropper stops picker
// if the picker is already selected.

const TEST_URI = `<style>body{background:red}</style>`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const {view, toolbox} = await openRuleView();
  const pickerStopped = toolbox.once("picker-stopped");

  await startPicker(toolbox);

  info("Get the background property from the rule-view");
  const property = getRuleViewProperty(view, "body", "background");
  const swatch = property.valueSpan.querySelector(".ruleview-colorswatch");
  ok(swatch, "Color swatch is displayed for the background property");

  info("Open the eyedropper from the colorpicker tooltip");
  await openEyedropper(view, swatch);

  info("Waiting for the picker-stopped event to be fired");
  await pickerStopped;

  ok(true, "picker-stopped event fired after eyedropper was clicked");
});

