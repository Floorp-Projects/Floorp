/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a color change in the color picker is committed when ENTER is
// pressed.

const TEST_URI = `
  <style type="text/css">
    body {
      border: 2em solid rgba(120, 120, 120, .5);
    }
  </style>
  Testing the color picker tooltip!
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {view} = yield openRuleView();

  let swatch = getRuleViewProperty(view, "body", "border").valueSpan
    .querySelector(".ruleview-colorswatch");
  yield testPressingEnterCommitsChanges(swatch, view);
});

function* testPressingEnterCommitsChanges(swatch, ruleView) {
  let cPicker = ruleView.tooltips.colorPicker;

  let onColorPickerReady = cPicker.once("ready");
  swatch.click();
  yield onColorPickerReady;

  yield simulateColorPickerChange(ruleView, cPicker, [0, 255, 0, .5], {
    selector: "body",
    name: "border-left-color",
    value: "rgba(0, 255, 0, 0.5)"
  });

  is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was updated");
  is(getRuleViewProperty(ruleView, "body", "border").valueSpan.textContent,
    "2em solid rgba(0, 255, 0, 0.5)",
    "The text of the border css property was updated");

  let onModified = ruleView.once("ruleview-changed");
  let spectrum = cPicker.spectrum;
  let onHidden = cPicker.tooltip.once("hidden");
  EventUtils.sendKey("RETURN", spectrum.element.ownerDocument.defaultView);
  yield onHidden;
  yield onModified;

  is((yield getComputedStyleProperty("body", null, "border-left-color")),
    "rgba(0, 255, 0, 0.5)", "The element's border was kept after RETURN");
  is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was kept after RETURN");
  is(getRuleViewProperty(ruleView, "body", "border").valueSpan.textContent,
    "2em solid rgba(0, 255, 0, 0.5)",
    "The text of the border css property was kept after RETURN");
}
