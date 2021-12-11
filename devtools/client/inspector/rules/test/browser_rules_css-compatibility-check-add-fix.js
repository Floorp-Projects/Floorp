/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test - Add fix for incompatible property
// For properties like "user-select", there exists an alias
// "-webkit-user-select", that is supported on all platform
// as a result of its popularity. If such a universally
// compatible alias exists, we shouldn't show compatibility
// warning for the base declaration.
// In this case "user-select" is marked compatible because the
// universally compatible alias "-webkit-user-select" exists
// alongside.

const TARGET_BROWSERS = [
  {
    // Chrome doesn't need any prefix for both user-select and text-size-adjust.
    id: "chrome",
    version: "84",
  },
  {
    // The safari_ios needs -webkit prefix for both properties.
    id: "safari_ios",
    version: "13",
  },
];

const TEST_URI = `
<style>
  div {
    color: green;
    background-color: black;
    user-select: none;
    text-size-adjust: none;
  }
</style>
<div>`;

const TEST_DATA_INITIAL = [
  {
    selector: "div",
    rules: [
      {},
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        "user-select": {
          value: "none",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.default,
        },
        "text-size-adjust": {
          value: "none",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.experimental,
        },
      },
    ],
  },
];

const TEST_DATA_FIX_USER_SELECT = [
  {
    selector: "div",
    rules: [
      {},
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        "user-select": { value: "none" },
        "-webkit-user-select": { value: "none" },
        "text-size-adjust": {
          value: "none",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.experimental,
        },
      },
    ],
  },
];

// text-size-adjust is an experimental property with aliases.
// Adding -webkit makes it compatible on all platforms but will
// still show an inline warning for its experimental status.
const TEST_DATA_FIX_EXPERIMENTAL_SUPPORTED = [
  {
    selector: "div",
    rules: [
      {},
      {
        color: { value: "green" },
        "background-color": { value: "black" },
        "user-select": { value: "none" },
        "-webkit-user-select": { value: "none" },
        "text-size-adjust": {
          value: "none",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE["experimental-supported"],
        },
      },
    ],
  },
];

add_task(async function() {
  await pushPref(
    "devtools.inspector.compatibility.target-browsers",
    JSON.stringify(TARGET_BROWSERS)
  );
  await pushPref(
    "devtools.inspector.ruleview.inline-compatibility-warning.enabled",
    true
  );
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await runCSSCompatibilityTests(view, inspector, TEST_DATA_INITIAL);

  info(
    'Add -webkit-user-select: "none" which solves the compatibility issue from user-select'
  );
  await addProperty(view, 1, "-webkit-user-select", "none");
  await runCSSCompatibilityTests(view, inspector, TEST_DATA_FIX_USER_SELECT);

  info(
    'Add -webkit-text-size-adjust: "none" fixing issue but leaving an inline warning of an experimental property'
  );
  await addProperty(view, 1, "-webkit-text-size-adjust", "none");
  await runCSSCompatibilityTests(
    view,
    inspector,
    TEST_DATA_FIX_EXPERIMENTAL_SUPPORTED
  );
});
