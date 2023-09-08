/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test cycling angle units in the rule view.

const TEST_URI = `
  <style type="text/css">
    .turn {
      filter: hue-rotate(1turn);
    }
    .deg {
      filter: hue-rotate(180deg);
    }
  </style>
  <body><div class=turn>Test turn</div><div class=deg>Test deg</div>cycling angle units in the rule view!</body>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await checkAngleCycling(inspector, view);
  await checkAngleCyclingPersist(inspector, view);
});

async function checkAngleCycling(inspector, view) {
  await selectNode(".turn", inspector);

  const container = (
    await getRuleViewProperty(view, ".turn", "filter", { wait: true })
  ).valueSpan;
  const valueNode = container.querySelector(".ruleview-angle");
  const win = view.styleWindow;

  // turn
  is(valueNode.textContent, "1turn", "Angle displayed as a turn value.");

  const tests = [
    {
      value: "360deg",
      comment: "Angle displayed as a degree value.",
    },
    {
      value: `${Math.round(Math.PI * 2 * 10000) / 10000}rad`,
      comment: "Angle displayed as a radian value.",
    },
    {
      value: "400grad",
      comment: "Angle displayed as a gradian value.",
    },
    {
      value: "1turn",
      comment: "Angle displayed as a turn value again.",
    },
  ];

  for (const test of tests) {
    await checkSwatchShiftClick(container, win, test.value, test.comment);
  }
}

async function checkAngleCyclingPersist(inspector, view) {
  await selectNode(".deg", inspector);
  let container = (
    await getRuleViewProperty(view, ".deg", "filter", { wait: true })
  ).valueSpan;
  let valueNode = container.querySelector(".ruleview-angle");
  const win = view.styleWindow;

  is(valueNode.textContent, "180deg", "Angle displayed as a degree value.");

  await checkSwatchShiftClick(
    container,
    win,
    `${Math.round(Math.PI * 10000) / 10000}rad`,
    "Angle displayed as a radian value."
  );

  // Select the .turn div and reselect the .deg div to see
  // if the new angle unit persisted
  await selectNode(".turn", inspector);
  await selectNode(".deg", inspector);

  // We have to query for the container and the swatch because
  // they've been re-generated
  container = (
    await getRuleViewProperty(view, ".deg", "filter", { wait: true })
  ).valueSpan;
  valueNode = container.querySelector(".ruleview-angle");
  is(
    valueNode.textContent,
    `${Math.round(Math.PI * 10000) / 10000}rad`,
    "Angle still displayed as a radian value."
  );
}

async function checkSwatchShiftClick(container, win, expectedValue, comment) {
  // Wait for 500ms before attempting a click to workaround frequent
  // intermittents.
  //
  // See intermittent bug at https://bugzilla.mozilla.org/show_bug.cgi?id=1721938
  // See potentially related bugs:
  // - browserLoaded + synthesizeMouse timeouts https://bugzilla.mozilla.org/show_bug.cgi?id=1727749
  // - mochitest general synthesize events issue https://bugzilla.mozilla.org/show_bug.cgi?id=1720248
  await wait(500);

  const swatch = container.querySelector(".ruleview-angleswatch");
  const valueNode = container.querySelector(".ruleview-angle");

  const onUnitChange = once(swatch, "unit-change");
  EventUtils.synthesizeMouseAtCenter(
    swatch,
    {
      type: "mousedown",
      shiftKey: true,
    },
    win
  );
  await onUnitChange;
  is(valueNode.textContent, expectedValue, comment);
}
