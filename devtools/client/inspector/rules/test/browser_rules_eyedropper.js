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

  const { inspector, view, toolbox } = await openRuleView();

  await runTest(inspector, view, false);

  info("Reload the page to restore the initial state");
  await navigateTo(url);

  info("Change toolbox host to WINDOW");
  await toolbox.switchHost("window");

  // Switching hosts is not correctly waiting when DevTools run in content frame
  // See Bug 1571421.
  await wait(1000);

  await runTest(inspector, view, true);
});

async function runTest(inspector, view, isWindowHost) {
  await selectNode("#div2", inspector);

  info("Get the background-color property from the rule-view");
  const property = getRuleViewProperty(view, "#div2", "background-color");
  const swatch = property.valueSpan.querySelector(".ruleview-colorswatch");
  ok(swatch, "Color swatch is displayed for the bg-color property");

  info("Open the eyedropper from the colorpicker tooltip");
  await openEyedropper(view, swatch);

  const tooltip = view.tooltips.getTooltip("colorPicker").tooltip;
  ok(
    !tooltip.isVisible(),
    "color picker tooltip is closed after opening eyedropper"
  );

  info("Test that pressing escape dismisses the eyedropper");
  await testESC(swatch, inspector);

  if (isWindowHost) {
    // The following code is only needed on linux otherwise the test seems to
    // timeout when clicking again on the swatch. Both the focus and the wait
    // seem needed to make it pass.
    // To be fixed in Bug 1571421.
    info("Ensure the swatch window is focused");
    const onWindowFocus = BrowserTestUtils.waitForEvent(
      swatch.ownerGlobal,
      "focus"
    );
    swatch.ownerGlobal.focus();
    await onWindowFocus;
  }

  info("Open the eyedropper again");
  await openEyedropper(view, swatch);

  info("Test that a color can be selected with the eyedropper");
  await testSelect(view, swatch, inspector);

  const onHidden = tooltip.once("hidden");
  tooltip.hide();
  await onHidden;
  ok(!tooltip.isVisible(), "color picker tooltip is closed");

  await waitForTick();
}

async function testESC(swatch, inspector) {
  info("Press escape");
  const onCanceled = new Promise(resolve => {
    inspector.inspectorFront.once("color-pick-canceled", resolve);
  });
  BrowserTestUtils.synthesizeKey(
    "VK_ESCAPE",
    {},
    gBrowser.selectedTab.linkedBrowser
  );
  await onCanceled;

  const color = swatch.style.backgroundColor;
  is(color, ORIGINAL_COLOR, "swatch didn't change after pressing ESC");
}

async function testSelect(view, swatch, inspector) {
  info("Click at x:10px y:10px");
  const onPicked = new Promise(resolve => {
    inspector.inspectorFront.once("color-picked", resolve);
  });
  // The change to the content is done async after rule view change
  const onRuleViewChanged = view.once("ruleview-changed");

  await safeSynthesizeMouseEventAtCenterInContentPage("#div1", {
    type: "mousemove",
  });
  await safeSynthesizeMouseEventAtCenterInContentPage("#div1", {
    type: "mousedown",
  });
  await safeSynthesizeMouseEventAtCenterInContentPage("#div1", {
    type: "mouseup",
  });

  await onPicked;
  await onRuleViewChanged;

  const color = swatch.style.backgroundColor;
  is(color, EXPECTED_COLOR, "swatch changed colors");

  ok(!swatch.eyedropperOpen, "swatch eye dropper is closed");
  ok(!swatch.activeSwatch, "no active swatch");

  is(
    await getComputedStyleProperty("div", null, "background-color"),
    EXPECTED_COLOR,
    "div's color set to body color after dropper"
  );
}
