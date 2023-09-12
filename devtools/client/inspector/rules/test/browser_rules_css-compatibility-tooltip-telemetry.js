/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test - Toggling rules linked to the element and the class
// Checking whether the compatibility warning icon is displayed
// correctly.
// If a rule is disabled, it is marked compatible to keep
// consistency with compatibility panel.
// We test both the compatible and incompatible rules here

const TEST_URI = `
<style>
  div {
    -moz-float-edge: content-box;
  }
</style>
<div></div>`;

const TEST_DATA = [
  {
    selector: "div",
    rules: [
      {},
      {
        "-moz-float-edge": {
          value: "content-box",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.deprecated,
        },
      },
    ],
  },
];

add_task(async function () {
  startTelemetry();

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  info("Check correctness of data by toggling tooltip open");
  await runCSSCompatibilityTests(view, inspector, TEST_DATA);

  checkResults();
});

function checkResults() {
  info(
    'Check the telemetry against "devtools.tooltip.shown" for label "css-compatibility" and ensure it is set'
  );
  checkTelemetry("devtools.tooltip.shown", "", 1, "css-compatibility");
}
