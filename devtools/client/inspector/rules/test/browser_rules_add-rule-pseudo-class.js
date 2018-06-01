/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
  [":focus"]
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
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
  if (pseudoClasses.length == 0) {
    return;
  }

  for (const pseudoClass of pseudoClasses) {
    switch (pseudoClass) {
      case ":hover":
        view.hoverCheckbox.click();
        await inspector.once("rule-view-refreshed");
        break;
      case ":active":
        view.activeCheckbox.click();
        await inspector.once("rule-view-refreshed");
        break;
      case ":focus":
        view.focusCheckbox.click();
        await inspector.once("rule-view-refreshed");
        break;
    }
  }
}

async function resetPseudoLocks(inspector, view) {
  if (!view.hoverCheckbox.checked &&
      !view.activeCheckbox.checked &&
      !view.focusCheckbox.checked) {
    return;
  }
  if (view.hoverCheckbox.checked) {
    view.hoverCheckbox.click();
    await inspector.once("rule-view-refreshed");
  }
  if (view.activeCheckbox.checked) {
    view.activeCheckbox.click();
    await inspector.once("rule-view-refreshed");
  }
  if (view.focusCheckbox.checked) {
    view.focusCheckbox.click();
    await inspector.once("rule-view-refreshed");
  }
}
