/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing the value of a CSS declaration in the Rule view is tracked.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
      font-family: "courier";
    }
  </style>
  <div>test</div>
`;

/*
  This object contains iteration steps to modify various CSS properties of the
  test element, keyed by property name,.
  Each value is an array which will be iterated over in order and the `value`
  property will be used to update the value of the property.
  The `add` and `remove` objects hold the expected values of the tracked declarations
  shown in the Changes panel. If `add` or `remove` are null, it means we don't expect
  any corresponding tracked declaration to show up in the Changes panel.
 */
const ITERATIONS = {
  color: [
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
  ],
  "font-family": [
    // Set a value with an opening quote, missing the closing one.
    // The closing quote should still appear in the "add" value.
    {
      value: '"ar',
      add: { value: '"ar"' },
      remove: { value: '"courier"' },
      // For some reason we need an additional flush the first time we set a
      // value with a quote. Since the ruleview is manually flushed when opened
      // openRuleView, we need to pass this information all the way down to the
      // setProperty helper.
      needsExtraFlush: true,
    },
    // Add an escaped character
    {
      value: '"ar\\i',
      add: { value: '"ar\\i"' },
      remove: { value: '"courier"' },
    },
    // Add some more text
    {
      value: '"ar\\ia',
      add: { value: '"ar\\ia"' },
      remove: { value: '"courier"' },
    },
    // Remove the backslash
    {
      value: '"aria',
      add: { value: '"aria"' },
      remove: { value: '"courier"' },
    },
    // Add the rest of the text, still no closing quote
    {
      value: '"arial',
      add: { value: '"arial"' },
      remove: { value: '"courier"' },
    },
    // Restoring the original value should clear tracked changes.
    {
      value: '"courier"',
      add: null,
      remove: null,
    },
  ],
};

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);

  await selectNode("div", inspector);

  const colorProp = getTextProperty(ruleView, 1, { color: "red" });
  await assertEditValue(ruleView, doc, store, colorProp, ITERATIONS.color);

  const fontFamilyProp = getTextProperty(ruleView, 1, {
    "font-family": '"courier"',
  });
  await assertEditValue(
    ruleView,
    doc,
    store,
    fontFamilyProp,
    ITERATIONS["font-family"]
  );
});

async function assertEditValue(ruleView, doc, store, prop, iterations) {
  let onTrackChange;
  for (const { value, add, needsExtraFlush, remove } of iterations) {
    onTrackChange = waitForDispatch(store, "TRACK_CHANGE");

    info(`Change the CSS declaration value to ${value}`);
    await setProperty(ruleView, prop, value, {
      flushCount: needsExtraFlush ? 2 : 1,
    });
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
        () => !getAddedDeclarations(doc).length,
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
        () => !getRemovedDeclarations(doc).length,
        "Removed declaration was cleared"
      );
    }
  }
}
