/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing the value of a CSS declaration in the Rule view is tracked.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <div></div>
`;

/*
  This array will be iterated over in order and the `value` property will be used to
  update the value of the first CSS declaration.
  The `add` and `remove` objects hold the expected values of the tracked declarations
  shown in the Changes panel. If `add` or `remove` are null, it means we don't expect
  any corresponding tracked declaration to show up in the Changes panel.
 */
const VALUE_CHANGE_ITERATIONS = [
  // No changes should be tracked if the value did not actually change.
  {
    value: "red",
    add: null,
    remove: null,
  },
  // Changing the priority flag "!important" should be tracked.
  {
    value: "red !important",
    add: { value: "red !important" },
    remove: { value: "red" },
  },
  // Repeated changes should still show the original value as the one removed.
  {
    value: "blue",
    add: { value: "blue" },
    remove: { value: "red" },
  },
  // Restoring the original value should clear tracked changes.
  {
    value: "red",
    add: null,
    remove: null,
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);
  const panel = doc.querySelector("#sidebar-panel-changes");

  await selectNode("div", inspector);
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const prop = rule.textProps[0];

  let onTrackChange;
  let removeValue;
  let addValue;

  for (const { value, add, remove } of VALUE_CHANGE_ITERATIONS) {
    onTrackChange = waitUntilAction(store, "TRACK_CHANGE");

    info(`Change the CSS declaration value to ${value}`);
    await setProperty(ruleView, prop, value);
    info("Wait for the change to be tracked");
    await onTrackChange;

    addValue = panel.querySelector(".diff-add .declaration-value");
    removeValue = panel.querySelector(".diff-remove .declaration-value");

    if (add) {
      is(addValue.textContent, add.value,
        `Added declaration has expected value: ${add.value}`);
      is(panel.querySelectorAll(".diff-add").length, 1,
        "Only one declaration was tracked as added.");
    } else {
      ok(!addValue, `Added declaration was cleared`);
    }

    if (remove) {
      is(removeValue.textContent, remove.value,
        `Removed declaration has expected value: ${remove.value}`);
      is(panel.querySelectorAll(".diff-remove").length, 1,
        "Only one declaration was tracked as removed.");
    } else {
      ok(!removeValue, `Removed declaration was cleared`);
    }
  }
});
