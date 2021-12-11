/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that cubic-bezier pickers appear when clicking on cubic-bezier
// swatches.

const TEST_URI = `
  <style type="text/css">
    div {
      animation: move 3s linear;
      transition: top 4s cubic-bezier(.1, 1.45, 1, -1.2);
    }
    .test {
      animation-timing-function: ease-in-out;
      transition-timing-function: ease-out;
    }
  </style>
  <div class="test">Testing the cubic-bezier tooltip!</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  const swatches = [];
  swatches.push(
    getRuleViewProperty(view, "div", "animation").valueSpan.querySelector(
      ".ruleview-bezierswatch"
    )
  );
  swatches.push(
    getRuleViewProperty(view, "div", "transition").valueSpan.querySelector(
      ".ruleview-bezierswatch"
    )
  );
  swatches.push(
    getRuleViewProperty(
      view,
      ".test",
      "animation-timing-function"
    ).valueSpan.querySelector(".ruleview-bezierswatch")
  );
  swatches.push(
    getRuleViewProperty(
      view,
      ".test",
      "transition-timing-function"
    ).valueSpan.querySelector(".ruleview-bezierswatch")
  );

  for (const swatch of swatches) {
    info("Testing that the cubic-bezier appears on cubicswatch click");
    await testAppears(view, swatch);
  }
});

async function testAppears(view, swatch) {
  ok(swatch, "The cubic-swatch exists");

  const bezier = view.tooltips.getTooltip("cubicBezier");
  ok(bezier, "The rule-view has the expected cubicBezier property");

  const bezierPanel = bezier.tooltip.panel;
  ok(bezierPanel, "The XUL panel for the cubic-bezier tooltip exists");

  const onBezierWidgetReady = bezier.once("ready");
  swatch.click();
  await onBezierWidgetReady;

  ok(true, "The cubic-bezier tooltip was shown on click of the cibuc swatch");
  ok(
    !inplaceEditor(swatch.parentNode),
    "The inplace editor wasn't shown as a result of the cibuc swatch click"
  );
  await hideTooltipAndWaitForRuleViewChanged(bezier, view);
}
