/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a rule with pseudo class locks on.

const TEST_URI = "<p id='element'>Test element</p>";

const EXPECTED_SELECTOR = "#element";
const TEST_DATA = [
  [],
  [":hover"],
  [":hover", ":active"],
  [":hover", ":active", ":focus"],
  [":active"],
  [":active", ":focus"],
  [":focus"],
  [":focus-within"],
  [":hover", ":focus-within"],
  [":hover", ":active", ":focus-within"],
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#element", inspector);

  for (const data of TEST_DATA) {
    await runTestData(inspector, view, data);
  }
});

async function runTestData(inspector, view, pseudoClasses) {
  await setPseudoLocks(inspector, view, pseudoClasses);

  const expected = EXPECTED_SELECTOR + pseudoClasses.join("");
  await addNewRuleAndDismissEditor(inspector, view, expected, 1);

  await resetPseudoLocks(inspector, view);
}

async function setPseudoLocks(inspector, view, pseudoClasses) {
  if (!pseudoClasses.length) {
    return;
  }

  for (const pseudoClass of pseudoClasses) {
    const checkbox = getPseudoClassCheckbox(view, pseudoClass);
    if (checkbox) {
      checkbox.click();
    }
    await inspector.once("rule-view-refreshed");
  }
}

async function resetPseudoLocks(inspector, view) {
  for (const checkbox of view.pseudoClassCheckboxes) {
    if (checkbox.checked) {
      checkbox.click();
      await inspector.once("rule-view-refreshed");
    }
  }
}
