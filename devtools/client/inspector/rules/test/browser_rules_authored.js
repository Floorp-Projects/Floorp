/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for as-authored styles.

add_task(function*() {
  yield basicTest();
  yield overrideTest();
  yield colorEditingTest();
});

function* createTestContent(style) {
  let content = `<style type="text/css">
      ${style}
      </style>
      <div id="testid" class="testclass">Styled Node</div>`;
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(content));

  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  return view;
}

function* basicTest() {
  let view = yield createTestContent("#testid {" +
                                     // Invalid property.
                                     "  something: random;" +
                                     // Invalid value.
                                     "  color: orang;" +
                                     // Override.
                                     "  background-color: blue;" +
                                     "  background-color: #f0c;" +
                                     "} ");

  let elementStyle = view._elementStyle;

  let expected = [
    {name: "something", overridden: true},
    {name: "color", overridden: true},
    {name: "background-color", overridden: true},
    {name: "background-color", overridden: false}
  ];

  let rule = elementStyle.rules[1];

  for (let i = 0; i < expected.length; ++i) {
    let prop = rule.textProps[i];
    is(prop.name, expected[i].name, "test name for prop " + i);
    is(prop.overridden, expected[i].overridden,
       "test overridden for prop " + i);
  }
}

function* overrideTest() {
  let gradientText1 = "(orange, blue);";
  let gradientText2 = "(pink, teal);";

  let view =
      yield createTestContent("#testid {" +
                              "  background-image: linear-gradient" +
                              gradientText1 +
                              "  background-image: -ms-linear-gradient" +
                              gradientText2 +
                              "  background-image: linear-gradient" +
                              gradientText2 +
                              "} ");

  let elementStyle = view._elementStyle;
  let rule = elementStyle.rules[1];

  // Initially the last property should be active.
  for (let i = 0; i < 3; ++i) {
    let prop = rule.textProps[i];
    is(prop.name, "background-image", "check the property name");
    is(prop.overridden, i !== 2, "check overridden for " + i);
  }

  rule.textProps[2].setEnabled(false);
  yield rule._applyingModifications;

  // Now the first property should be active.
  for (let i = 0; i < 3; ++i) {
    let prop = rule.textProps[i];
    is(prop.overridden || !prop.enabled, i !== 0,
       "post-change check overridden for " + i);
  }
}

function* colorEditingTest() {
  let colors = [
    {name: "hex", text: "#f0c", result: "#0f0"},
    {name: "rgb", text: "rgb(0,128,250)", result: "rgb(0, 255, 0)"},
    // Test case preservation.
    {name: "hex", text: "#F0C", result: "#0F0"},
  ];

  Services.prefs.setCharPref("devtools.defaultColorUnit", "authored");

  for (let color of colors) {
    let view = yield createTestContent("#testid {" +
                                       "  color: " + color.text + ";" +
                                       "} ");

    let cPicker = view.tooltips.colorPicker;
    let swatch = getRuleViewProperty(view, "#testid", "color").valueSpan
        .querySelector(".ruleview-colorswatch");
    let onShown = cPicker.tooltip.once("shown");
    swatch.click();
    yield onShown;

    let testNode = yield getNode("#testid");

    yield simulateColorPickerChange(view, cPicker, [0, 255, 0, 1], {
      element: testNode,
      name: "color",
      value: "rgb(0, 255, 0)"
    });

    let spectrum = yield cPicker.spectrum;
    let onHidden = cPicker.tooltip.once("hidden");
    EventUtils.sendKey("RETURN", spectrum.element.ownerDocument.defaultView);
    yield onHidden;

    is(getRuleViewPropertyValue(view, "#testid", "color"), color.result,
       "changing the color preserved the unit for " + color.name);
  }
}
