/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that color pickers stops following the pointer if the pointer is
// released outside the tooltip frame (bug 1160720).

const TEST_URI = "<body style='color: red'>Test page for bug 1160720";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {view} = await openRuleView();

  const cSwatch = getRuleViewProperty(view, "element", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  const picker = await openColorPickerForSwatch(cSwatch, view);
  const spectrum = picker.spectrum;
  const change = spectrum.once("changed");

  info("Pressing mouse down over color picker.");
  const onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeMouseAtCenter(spectrum.dragger, {
    type: "mousedown",
  }, spectrum.dragger.ownerDocument.defaultView);
  await onRuleViewChanged;

  const value = await change;
  info(`Color changed to ${value} on mousedown.`);

  // If the mousemove below fails to detect that the button is no longer pressed
  // the spectrum will update and emit changed event synchronously after calling
  // synthesizeMouse so this handler is executed before the test ends.
  spectrum.once("changed", newValue => {
    is(newValue, value, "Value changed on mousemove without a button pressed.");
  });

  // Releasing the button pressed by mousedown above on top of a different frame
  // does not make sense in this test as EventUtils doesn't preserve the context
  // i.e. the buttons that were pressed down between events.

  info("Moving mouse over color picker without any buttons pressed.");

  EventUtils.synthesizeMouse(spectrum.dragger, 10, 10, {
    // -1 = no buttons are pressed down
    button: -1,
    type: "mousemove",
  }, spectrum.dragger.ownerDocument.defaultView);
});

async function openColorPickerForSwatch(swatch, view) {
  const cPicker = view.tooltips.getTooltip("colorPicker");
  ok(cPicker, "The rule-view has the expected colorPicker property");

  const cPickerPanel = cPicker.tooltip.panel;
  ok(cPickerPanel, "The XUL panel for the color picker exists");

  const onColorPickerReady = cPicker.once("ready");
  swatch.click();
  await onColorPickerReady;

  ok(true, "The color picker was shown on click of the color swatch");

  return cPicker;
}
