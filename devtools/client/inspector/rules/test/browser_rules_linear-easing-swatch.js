/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that linear easing widget pickers appear when clicking on linear
// swatches.

const TEST_URI = `
  <style type="text/css">
    div {
      animation: move 3s linear(0, 0.2, 1);
      transition: top 4s linear(0 10%, 0.5 20% 80%, 0 90%);
    }
    .test {
      animation-timing-function: linear(0, 1 50% 100%);
      transition-timing-function: linear(1 -10%, 0, -1 110%);
    }
  </style>
  <div class="test">Testing the linear easing tooltip!</div>
`;

add_task(async function testSwatches() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  const tooltip = view.tooltips.getTooltip("linearEaseFunction");
  ok(tooltip, "The rule-view has the expected linear tooltip");
  const panel = tooltip.tooltip.panel;
  ok(panel, "The XUL panel for the linear tooltip exists");

  const expectedData = [
    {
      selector: "div",
      property: "animation",
      expectedPoints: [
        [0, 0],
        [0.5, 0.2],
        [1, 1],
      ],
    },
    {
      selector: "div",
      property: "transition",
      expectedPoints: [
        [0.1, 0],
        [0.2, 0.5],
        [0.8, 0.5],
        [0.9, 0],
      ],
    },
    {
      selector: ".test",
      property: "animation-timing-function",
      expectedPoints: [
        [0, 0],
        [0.5, 1],
        [1, 1],
      ],
    },
    {
      selector: ".test",
      property: "transition-timing-function",
      expectedPoints: [
        [-0.1, 1],
        [0.5, 0],
        [1.1, -1],
      ],
    },
  ];

  for (const { selector, property, expectedPoints } of expectedData) {
    const messagePrefix = `[${selector}|${property}]`;
    const swatch = getRuleViewLinearEasingSwatch(view, selector, property);
    ok(swatch, `${messagePrefix} the swatch exists`);

    const onWidgetReady = tooltip.once("ready");
    swatch.click();
    await onWidgetReady;
    ok(true, `${messagePrefix} clicking the swatch displayed the tooltip`);

    ok(
      !inplaceEditor(swatch.parentNode),
      `${messagePrefix} inplace editor wasn't shown`
    );

    checkChartState(panel, expectedPoints);

    await hideTooltipAndWaitForRuleViewChanged(tooltip, view);
  }
});

add_task(async function testChart() {
  await pushPref("ui.prefersReducedMotion", 0);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  const tooltip = view.tooltips.getTooltip("linearEaseFunction");
  ok(tooltip, "The rule-view has the expected linear tooltip");
  const panel = tooltip.tooltip.panel;
  ok(panel, "The XUL panel for the linear tooltip exists");

  const selector = ".test";
  const property = "animation-timing-function";

  const swatch = getRuleViewLinearEasingSwatch(view, selector, property);
  const onWidgetReady = tooltip.once("ready");
  swatch.click();
  await onWidgetReady;
  const widget = await tooltip.widget;

  const svgEl = panel.querySelector(`svg.chart`);
  const svgRect = svgEl.getBoundingClientRect();

  checkChartState(
    panel,
    [
      [0, 0],
      [0.5, 1],
      [1, 1],
    ],
    "testChart - initial state:"
  );

  info("Check that double clicking a point removes it");
  const middlePoint = panel.querySelector(
    `svg.chart .control-points-group .control-point:nth-of-type(2)`
  );
  let onWidgetUpdated = widget.once("updated");
  let onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendMouseEvent(
    { type: "dblclick" },
    middlePoint,
    widget.parent.ownerGlobal
  );

  let newValue = await onWidgetUpdated;
  is(newValue, `linear(0 0%, 1 100%)`);
  checkChartState(
    panel,
    [
      [0, 0],
      [1, 1],
    ],
    "testChart - after point removed:"
  );
  await onRuleViewChanged;
  await checkRuleView(view, selector, property, newValue);

  info(
    "Check that double clicking a point when there are only 2 points on the line does not remove it"
  );
  const timeoutRes = Symbol();
  let onTimeout = wait(1000).then(() => timeoutRes);
  onWidgetUpdated = widget.once("updated");
  EventUtils.sendMouseEvent(
    { type: "dblclick" },
    panel.querySelector(`svg.chart .control-points-group .control-point`),
    widget.parent.ownerGlobal
  );
  let raceWinner = await Promise.race([onWidgetUpdated, onTimeout]);
  is(
    raceWinner,
    timeoutRes,
    "The widget wasn't updated after double clicking one of the 2 last points"
  );
  checkChartState(
    panel,
    [
      [0, 0],
      [1, 1],
    ],
    "testChart - no point removed:"
  );

  info("Check that double clicking on the svg does add a point");
  onWidgetUpdated = widget.once("updated");
  onRuleViewChanged = view.once("ruleview-changed");
  // Clicking on svg center with shiftKey so it snaps to the grid
  EventUtils.synthesizeMouseAtCenter(
    panel.querySelector(`svg.chart`),
    { clickCount: 2, shiftKey: true },
    widget.parent.ownerGlobal
  );

  newValue = await onWidgetUpdated;
  is(
    newValue,
    `linear(0 0%, 0.5 50%, 1 100%)`,
    "Widget was updated with expected value"
  );
  checkChartState(
    panel,
    [
      [0, 0],
      [0.5, 0.5],
      [1, 1],
    ],
    "testChart - new point added"
  );
  await onRuleViewChanged;
  await checkRuleView(view, selector, property, newValue);

  info("Check that points can be moved");
  onWidgetUpdated = widget.once("updated");
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeMouseAtCenter(
    panel.querySelector(`svg.chart .control-points-group .control-point`),
    { type: "mousedown" },
    widget.parent.ownerGlobal
  );

  EventUtils.synthesizeMouse(
    svgEl,
    svgRect.width / 3,
    svgRect.height / 3,
    { type: "mousemove", shiftKey: true },
    widget.parent.ownerGlobal
  );
  newValue = await onWidgetUpdated;
  is(
    newValue,
    `linear(0.7 30%, 0.5 50%, 1 100%)`,
    "Widget was updated with expected value"
  );
  checkChartState(
    panel,
    [
      [0.3, 0.7],
      [0.5, 0.5],
      [1, 1],
    ],
    "testChart - point moved"
  );
  await onRuleViewChanged;
  await checkRuleView(view, selector, property, newValue);

  info("Check that the points can be moved past the next point");
  onWidgetUpdated = widget.once("updated");
  onRuleViewChanged = view.once("ruleview-changed");

  // the second point is at 50%, so simulate a mousemove all the way to the right (which
  // should be ~100%)
  EventUtils.synthesizeMouse(
    svgEl,
    svgRect.width,
    svgRect.height / 3,
    { type: "mousemove", shiftKey: true },
    widget.parent.ownerGlobal
  );
  newValue = await onWidgetUpdated;
  is(
    newValue,
    `linear(0.7 50%, 0.5 50%, 1 100%)`,
    "point wasn't moved past the next point (50%)"
  );
  checkChartState(
    panel,
    [
      [0.5, 0.7],
      [0.5, 0.5],
      [1, 1],
    ],
    "testChart - point moved constrained by next point"
  );
  await onRuleViewChanged;
  await checkRuleView(view, selector, property, newValue);

  info("Stop dragging");
  EventUtils.synthesizeMouseAtCenter(
    svgEl,
    { type: "mouseup" },
    widget.parent.ownerGlobal
  );

  onTimeout = wait(1000).then(() => timeoutRes);
  onWidgetUpdated = widget.once("updated");
  EventUtils.synthesizeMouseAtCenter(
    svgEl,
    { type: "mousemove" },
    widget.parent.ownerGlobal
  );
  raceWinner = await Promise.race([onWidgetUpdated, onTimeout]);
  is(raceWinner, timeoutRes, "Dragging is disabled after mouseup");

  info("Add another point, which should be the first one for the line");
  onWidgetUpdated = widget.once("updated");
  onRuleViewChanged = view.once("ruleview-changed");

  EventUtils.synthesizeMouse(
    svgEl,
    svgRect.width / 3,
    svgRect.height - 1,
    { clickCount: 2, shiftKey: true },
    widget.parent.ownerGlobal
  );

  newValue = await onWidgetUpdated;
  is(
    newValue,
    `linear(0 30%, 0.7 50%, 0.5 50%, 1 100%)`,
    "Widget was updated with expected value"
  );
  checkChartState(
    panel,
    [
      [0.3, 0],
      [0.5, 0.7],
      [0.5, 0.5],
      [1, 1],
    ],
    "testChart - point added at beginning"
  );
  await onRuleViewChanged;
  await checkRuleView(view, selector, property, newValue);

  info("Check that the points can't be moved past previous point");
  onWidgetUpdated = widget.once("updated");
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeMouseAtCenter(
    panel.querySelector(
      `svg.chart .control-points-group .control-point:nth-of-type(2)`
    ),
    { type: "mousedown" },
    widget.parent.ownerGlobal
  );

  EventUtils.synthesizeMouse(
    svgEl,
    0,
    svgRect.height / 3,
    { type: "mousemove", shiftKey: true },
    widget.parent.ownerGlobal
  );
  newValue = await onWidgetUpdated;
  is(
    newValue,
    `linear(0 30%, 0.7 30%, 0.5 50%, 1 100%)`,
    "point wasn't moved past previous point (30%)"
  );
  checkChartState(
    panel,
    [
      [0.3, 0],
      [0.3, 0.7],
      [0.5, 0.5],
      [1, 1],
    ],
    "testChart - point moved constrained by previous point"
  );
  await onRuleViewChanged;
  await checkRuleView(view, selector, property, newValue);

  info("Stop dragging");
  EventUtils.synthesizeMouseAtCenter(
    svgEl,
    { type: "mouseup" },
    widget.parent.ownerGlobal
  );

  info(
    "Check that timing preview is destroyed if prefers-reduced-motion gets enabled"
  );
  const getTimingFunctionPreview = () =>
    panel.querySelector(".timing-function-preview");
  ok(getTimingFunctionPreview(), "By default, timing preview is visible");
  info("Enable prefersReducedMotion");
  await pushPref("ui.prefersReducedMotion", 1);
  await waitFor(() => !getTimingFunctionPreview());
  ok(true, "timing preview was removed after enabling prefersReducedMotion");

  info("Disable prefersReducedMotion");
  await pushPref("ui.prefersReducedMotion", 0);
  await waitFor(() => getTimingFunctionPreview());
  ok(
    true,
    "timing preview was added back after disabling prefersReducedMotion"
  );

  info("Hide tooltip with escape to cancel modification");
  const onHidden = tooltip.tooltip.once("hidden");
  const onModifications = view.once("ruleview-changed");
  focusAndSendKey(widget.parent.ownerDocument.defaultView, "ESCAPE");
  await onHidden;
  await onModifications;

  await checkRuleView(
    view,
    selector,
    property,
    "linear(0, 1 50% 100%)",
    "linear(0 0%, 1 50%, 1 100%)"
  );
});

/**
 * Check that the svg chart line and control points are placed where we expect them.
 *
 * @param {ToolipPanel} panel
 * @param {Array<Array<Number>>} expectedPoints: Array of coordinated
 * @param {String} messagePrefix
 */
function checkChartState(panel, expectedPoints, messagePrefix = "") {
  const svgLine = panel.querySelector("svg.chart .chart-linear");
  is(
    svgLine.getAttribute("points"),
    expectedPoints.map(([x, y]) => `${x},${1 - y}`).join(" "),
    `${messagePrefix} line has the expected points`
  );

  const controlPoints = panel.querySelectorAll(
    `svg.chart .control-points-group .control-point`
  );

  is(
    controlPoints.length,
    expectedPoints.length,
    `${messagePrefix} the expected number of control points were created`
  );
  controlPoints.forEach((controlPoint, i) => {
    is(
      parseFloat(controlPoint.getAttribute("cx")),
      expectedPoints[i][0],
      `${messagePrefix} Control point ${i} has correct cx`
    );
    is(
      parseFloat(controlPoint.getAttribute("cy")),
      // XXX work around floating point issues
      Math.round((1 - expectedPoints[i][1]) * 10) / 10,
      `${messagePrefix} Control point ${i} has correct cy`
    );
  });
}

/**
 * Checks if the property in the rule view has the expected state
 *
 * @param {RuleView} view
 * @param {String} selector
 * @param {String} property
 * @param {String} expectedLinearValue: Expected value in the rule view
 * @param {String} expectedComputedLinearValue: Expected computed value. Defaults to expectedLinearValue.
 * @returns {Element|null}
 */
async function checkRuleView(
  view,
  selector,
  property,
  expectedLinearValue,
  expectedComputedLinearValue = expectedLinearValue
) {
  await waitForComputedStyleProperty(
    selector,
    null,
    property,
    expectedComputedLinearValue
  );

  is(
    getRuleViewProperty(view, selector, property).valueSpan.textContent,
    expectedLinearValue,
    `${selector} ${property} has expected value`
  );
  const swatch = getRuleViewLinearEasingSwatch(view, selector, property);
  is(
    swatch.getAttribute("data-linear"),
    expectedLinearValue,
    `${selector} ${property} swatch has expected "data-linear" attribute`
  );
}

/**
 * Returns the linear easing swatch for a rule (defined by its selector), and a property.
 *
 * @param {RuleView} view
 * @param {String} selector
 * @param {String} property
 * @returns {Element|null}
 */
function getRuleViewLinearEasingSwatch(view, selector, property) {
  return getRuleViewProperty(view, selector, property).valueSpan.querySelector(
    ".ruleview-lineareasingswatch"
  );
}
