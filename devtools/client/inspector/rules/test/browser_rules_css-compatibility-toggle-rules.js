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
    color: green;
    background-color: black;
    cursor: pointer;
  }
</style>
<div class="test-inline" style="color:pink; user-select:none;"></div>
<div class="test-class-linked"></div>`;

const TEST_DATA_INITIAL = [
  {
    selector: ".test-class-linked",
    rules: [
      {},
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        cursor: {
          value: "pointer",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.default,
        },
      },
    ],
  },
  {
    selector: ".test-inline",
    rules: [
      {
        color: { value: "pink" },
        "user-select": {
          value: "none",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.default,
        },
      },
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        cursor: {
          value: "pointer",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.default,
        },
      },
    ],
  },
];

const TEST_DATA_TOGGLE_CLASS_DECLARATION = [
  {
    selector: ".test-class-linked",
    rules: [
      {},
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        cursor: { value: "pointer" },
      },
    ],
  },
  {
    selector: ".test-inline",
    rules: [
      {
        color: { value: "pink" },
        "user-select": {
          value: "none",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.default,
        },
      },
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        cursor: { value: "pointer" },
      },
    ],
  },
];

const TEST_DATA_TOGGLE_INLINE = [
  {
    selector: ".test-class-linked",
    rules: [
      {},
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        cursor: { value: "pointer" },
      },
    ],
  },
  {
    selector: ".test-inline",
    rules: [
      {
        color: { value: "pink" },
        "user-select": { value: "none" },
      },
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        cursor: { value: "pointer" },
      },
    ],
  },
];

add_task(async function() {
  await pushPref(
    "devtools.inspector.ruleview.inline-compatibility-warning.enabled",
    true
  );
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  const cursor = { cursor: "pointer" };
  const userSelect = { "user-select": "none" };

  await runCSSCompatibilityTests(view, inspector, TEST_DATA_INITIAL);

  info(
    'Disable cursor: "pointer" which is not cross browser compatible declaration'
  );
  await toggleDeclaration(view, 1, cursor);
  await runCSSCompatibilityTests(
    view,
    inspector,
    TEST_DATA_TOGGLE_CLASS_DECLARATION
  );

  info(
    'Toggle inline declaration "user-select": "none" and check the compatibility status'
  );
  await selectNode(".test-inline", inspector);
  await toggleDeclaration(view, 0, userSelect);
  await runCSSCompatibilityTests(view, inspector, TEST_DATA_TOGGLE_INLINE);
});
