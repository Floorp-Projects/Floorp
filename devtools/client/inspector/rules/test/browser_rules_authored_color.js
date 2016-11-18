/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for as-authored color styles.

/**
 * Array of test color objects:
 *   {String} name: name of the used & expected color format.
 *   {String} id: id of the element that will be created to test this color.
 *   {String} color: initial value of the color property applied to the test element.
 *   {String} result: expected value of the color property after edition.
 */
const colors = [
  {name: "hex", id: "test1", color: "#f0c", result: "#0f0"},
  {name: "rgb", id: "test2", color: "rgb(0,128,250)", result: "rgb(0, 255, 0)"},
  // Test case preservation.
  {name: "hex", id: "test3", color: "#F0C", result: "#0F0"},
];

add_task(function* () {
  Services.prefs.setCharPref("devtools.defaultColorUnit", "authored");

  let html = "";
  for (let {color, id} of colors) {
    html += `<div id="${id}" style="color: ${color}">Styled Node</div>`;
  }

  let tab = yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(html));

  let {inspector, view} = yield openRuleView();

  for (let color of colors) {
    let cPicker = view.tooltips.colorPicker;
    let selector = "#" + color.id;
    yield selectNode(selector, inspector);

    let swatch = getRuleViewProperty(view, "element", "color").valueSpan
        .querySelector(".ruleview-colorswatch");
    let onColorPickerReady = cPicker.once("ready");
    swatch.click();
    yield onColorPickerReady;

    yield simulateColorPickerChange(view, cPicker, [0, 255, 0, 1], {
      selector,
      name: "color",
      value: "rgb(0, 255, 0)"
    });

    let spectrum = cPicker.spectrum;
    let onHidden = cPicker.tooltip.once("hidden");
    // Validating the color change ends up updating the rule view twice
    let onRuleViewChanged = waitForNEvents(view, "ruleview-changed", 2);
    focusAndSendKey(spectrum.element.ownerDocument.defaultView, "RETURN");
    yield onHidden;
    yield onRuleViewChanged;

    is(getRuleViewPropertyValue(view, "element", "color"), color.result,
       "changing the color preserved the unit for " + color.name);
  }

  let target = TargetFactory.forTab(tab);
  yield gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});
