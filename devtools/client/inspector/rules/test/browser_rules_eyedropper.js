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

add_task(function* () {
  info("Add the test tab, open the rule-view and select the test node");
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {testActor, inspector, view} = yield openRuleView();
  yield selectNode("#div2", inspector);

  info("Get the background-color property from the rule-view");
  let property = getRuleViewProperty(view, "#div2", "background-color");
  let swatch = property.valueSpan.querySelector(".ruleview-colorswatch");
  ok(swatch, "Color swatch is displayed for the bg-color property");

  info("Open the eyedropper from the colorpicker tooltip");
  yield openEyedropper(view, swatch);

  let tooltip = view.tooltips.getTooltip("colorPicker").tooltip;
  ok(!tooltip.isVisible(), "color picker tooltip is closed after opening eyedropper");

  info("Test that pressing escape dismisses the eyedropper");
  yield testESC(swatch, inspector, testActor);

  info("Open the eyedropper again");
  yield openEyedropper(view, swatch);

  info("Test that a color can be selected with the eyedropper");
  yield testSelect(view, swatch, inspector, testActor);

  let onHidden = tooltip.once("hidden");
  tooltip.hide();
  yield onHidden;
  ok(!tooltip.isVisible(), "color picker tooltip is closed");

  yield waitForTick();
});

function* testESC(swatch, inspector, testActor) {
  info("Press escape");
  let onCanceled = new Promise(resolve => {
    inspector.inspector.once("color-pick-canceled", resolve);
  });
  yield testActor.synthesizeKey({key: "VK_ESCAPE", options: {}});
  yield onCanceled;

  let color = swatch.style.backgroundColor;
  is(color, ORIGINAL_COLOR, "swatch didn't change after pressing ESC");
}

function* testSelect(view, swatch, inspector, testActor) {
  info("Click at x:10px y:10px");
  let onPicked = new Promise(resolve => {
    inspector.inspector.once("color-picked", resolve);
  });
  // The change to the content is done async after rule view change
  let onRuleViewChanged = view.once("ruleview-changed");

  yield testActor.synthesizeMouse({selector: "html", x: 10, y: 10,
                                   options: {type: "mousemove"}});
  yield testActor.synthesizeMouse({selector: "html", x: 10, y: 10,
                                   options: {type: "mousedown"}});
  yield testActor.synthesizeMouse({selector: "html", x: 10, y: 10,
                                   options: {type: "mouseup"}});

  yield onPicked;
  yield onRuleViewChanged;

  let color = swatch.style.backgroundColor;
  is(color, EXPECTED_COLOR, "swatch changed colors");

  is((yield getComputedStyleProperty("div", null, "background-color")),
     EXPECTED_COLOR,
     "div's color set to body color after dropper");
}

function* openEyedropper(view, swatch) {
  let tooltip = view.tooltips.getTooltip("colorPicker").tooltip;

  info("Click on the swatch");
  let onColorPickerReady = view.tooltips.getTooltip("colorPicker").once("ready");
  swatch.click();
  yield onColorPickerReady;

  let dropperButton = tooltip.container.querySelector("#eyedropper-button");

  info("Click on the eyedropper icon");
  let onOpened = tooltip.once("eyedropper-opened");
  dropperButton.click();
  yield onOpened;
}
