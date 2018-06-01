/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test opening the eyedropper from the color picker. Pressing escape to close it, and
// clicking the page to select a color.

const TEST_URI = `
  <style type="text/css">
    body {
      background-color: white;
      padding: 0px
    }

    #div1 {
      background-color: #ff5;
      width: 20px;
      height: 20px;
    }

    #div2 {
      margin-left: 20px;
      width: 20px;
      height: 20px;
      background-color: #f09;
    }
  </style>
  <body><div id="div1"></div><div id="div2"></div></body>
`;

// #f09
const ORIGINAL_COLOR = "rgb(255, 0, 153)";
// #ff5
const EXPECTED_COLOR = "rgb(255, 255, 85)";

registerCleanupFunction(() => {
  // Restore the default Toolbox host position after the test.
  Services.prefs.clearUserPref("devtools.toolbox.host");
});

add_task(async function() {
  info("Add the test tab, open the rule-view and select the test node");

  const url = "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI);
  await addTab(url);

  const {testActor, inspector, view, toolbox} = await openRuleView();

  await runTest(testActor, inspector, view);

  info("Reload the page to restore the initial state");
  await navigateTo(inspector, url);

  info("Change toolbox host to WINDOW");
  await toolbox.switchHost("window");

  await runTest(testActor, inspector, view);
});

async function runTest(testActor, inspector, view) {
  await selectNode("#div2", inspector);

  info("Get the background-color property from the rule-view");
  const property = getRuleViewProperty(view, "#div2", "background-color");
  const swatch = property.valueSpan.querySelector(".ruleview-colorswatch");
  ok(swatch, "Color swatch is displayed for the bg-color property");

  info("Open the eyedropper from the colorpicker tooltip");
  await openEyedropper(view, swatch);

  const tooltip = view.tooltips.getTooltip("colorPicker").tooltip;
  ok(!tooltip.isVisible(), "color picker tooltip is closed after opening eyedropper");

  info("Test that pressing escape dismisses the eyedropper");
  await testESC(swatch, inspector, testActor);

  info("Open the eyedropper again");
  await openEyedropper(view, swatch);

  info("Test that a color can be selected with the eyedropper");
  await testSelect(view, swatch, inspector, testActor);

  const onHidden = tooltip.once("hidden");
  tooltip.hide();
  await onHidden;
  ok(!tooltip.isVisible(), "color picker tooltip is closed");

  await waitForTick();
}

async function testESC(swatch, inspector, testActor) {
  info("Press escape");
  const onCanceled = new Promise(resolve => {
    inspector.inspector.once("color-pick-canceled", resolve);
  });
  await testActor.synthesizeKey({key: "VK_ESCAPE", options: {}});
  await onCanceled;

  const color = swatch.style.backgroundColor;
  is(color, ORIGINAL_COLOR, "swatch didn't change after pressing ESC");
}

async function testSelect(view, swatch, inspector, testActor) {
  info("Click at x:10px y:10px");
  const onPicked = new Promise(resolve => {
    inspector.inspector.once("color-picked", resolve);
  });
  // The change to the content is done async after rule view change
  const onRuleViewChanged = view.once("ruleview-changed");

  await testActor.synthesizeMouse({selector: "html", x: 10, y: 10,
                                   options: {type: "mousemove"}});
  await testActor.synthesizeMouse({selector: "html", x: 10, y: 10,
                                   options: {type: "mousedown"}});
  await testActor.synthesizeMouse({selector: "html", x: 10, y: 10,
                                   options: {type: "mouseup"}});

  await onPicked;
  await onRuleViewChanged;

  const color = swatch.style.backgroundColor;
  is(color, EXPECTED_COLOR, "swatch changed colors");

  ok(!swatch.eyedropperOpen, "swatch eye dropper is closed");
  ok(!swatch.activeSwatch, "no active swatch");

  is((await getComputedStyleProperty("div", null, "background-color")),
     EXPECTED_COLOR,
     "div's color set to body color after dropper");
}

