/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that color selection respects the user pref.

const TEST_URI = `
  <style type='text/css'>
    #testid {
      color: blue;
    }
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

add_task(async function() {
  const TESTS = [
    {name: "hex", result: "#0f0"},
    {name: "rgb", result: "rgb(0, 255, 0)"}
  ];

  for (const {name, result} of TESTS) {
    info("starting test for " + name);
    Services.prefs.setCharPref("devtools.defaultColorUnit", name);

    const tab = await addTab("data:text/html;charset=utf-8," +
                           encodeURIComponent(TEST_URI));
    const {inspector, view} = await openRuleView();

    await selectNode("#testid", inspector);
    await basicTest(view, name, result);

    const target = TargetFactory.forTab(tab);
    await gDevTools.closeToolbox(target);
    gBrowser.removeCurrentTab();
  }
});

async function basicTest(view, name, result) {
  const cPicker = view.tooltips.getTooltip("colorPicker");
  const swatch = getRuleViewProperty(view, "#testid", "color").valueSpan
      .querySelector(".ruleview-colorswatch");
  const onColorPickerReady = cPicker.once("ready");
  swatch.click();
  await onColorPickerReady;

  await simulateColorPickerChange(view, cPicker, [0, 255, 0, 1], {
    selector: "#testid",
    name: "color",
    value: "rgb(0, 255, 0)"
  });

  const spectrum = cPicker.spectrum;
  const onHidden = cPicker.tooltip.once("hidden");
  // Validating the color change ends up updating the rule view twice
  const onRuleViewChanged = waitForNEvents(view, "ruleview-changed", 2);
  focusAndSendKey(spectrum.element.ownerDocument.defaultView, "RETURN");
  await onHidden;
  await onRuleViewChanged;

  is(getRuleViewPropertyValue(view, "#testid", "color"), result,
     "changing the color used the " + name + " unit");
}
