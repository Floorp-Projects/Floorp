/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a color change in the color picker is reverted when ESC is
// pressed.

const TEST_URI = `
  <style type="text/css">
    body {
      background-color: #EDEDED;
    }
  </style>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {view} = await openRuleView();
  await testPressingEscapeRevertsChanges(view);
});

async function testPressingEscapeRevertsChanges(view) {
  const {swatch, propEditor, cPicker} = await openColorPickerAndSelectColor(view,
    1, 0, [0, 0, 0, 1], {
      selector: "body",
      name: "background-color",
      value: "rgb(0, 0, 0)"
    });

  is(swatch.style.backgroundColor, "rgb(0, 0, 0)",
    "The color swatch's background was updated");
  is(propEditor.valueSpan.textContent, "#000",
    "The text of the background-color css property was updated");

  const spectrum = cPicker.spectrum;

  info("Pressing ESCAPE to close the tooltip");
  const onHidden = cPicker.tooltip.once("hidden");
  const onModifications = view.once("ruleview-changed");
  EventUtils.sendKey("ESCAPE", spectrum.element.ownerDocument.defaultView);
  await onHidden;
  await onModifications;

  await waitForComputedStyleProperty("body", null, "background-color",
    "rgb(237, 237, 237)");
  is(propEditor.valueSpan.textContent, "#EDEDED",
    "Got expected property value.");
}
