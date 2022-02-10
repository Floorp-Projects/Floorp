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

  await selectNode("div", inspector);
  const prop = getTextProperty(ruleView, 1, { color: "red" });

  let onTrackChange;

  for (const { value, add, remove } of VALUE_CHANGE_ITERATIONS) {
    onTrackChange = waitUntilAction(store, "TRACK_CHANGE");

    info(`Change the CSS declaration value to ${value}`);
    await setProperty(ruleView, prop, value);
    info("Wait for the change to be tracked");
    await onTrackChange;

    if (add) {
      await waitFor(() => {
        const decl = getAddedDeclarations(doc);
        return decl.length == 1 && decl[0].value == add.value;
      }, "Only one declaration was tracked as added.");
      const addDecl = getAddedDeclarations(doc);
      is(
        addDecl[0].value,
        add.value,
        `Added declaration has expected value: ${add.value}`
      );
    } else {
      await waitFor(
        () => getAddedDeclarations(doc).length == 0,
        "Added declaration was cleared"
      );
    }

    if (remove) {
      await waitFor(
        () => getRemovedDeclarations(doc).length == 1,
        "Only one declaration was tracked as removed."
      );
      const removeDecl = getRemovedDeclarations(doc);
      is(
        removeDecl[0].value,
        remove.value,
        `Removed declaration has expected value: ${remove.value}`
      );
    } else {
      await waitFor(
        () => getRemovedDeclarations(doc).length == 0,
        "Removed declaration was cleared"
      );
    }
  }
});
