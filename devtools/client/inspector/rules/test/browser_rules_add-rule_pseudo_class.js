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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#element", inspector);

  for (let data of TEST_DATA) {
    yield runTestData(inspector, view, data);
  }
});

function* runTestData(inspector, view, pseudoClasses) {
  yield setPseudoLocks(inspector, view, pseudoClasses);
  yield addNewRule(inspector, view);
  yield testNewRule(view, pseudoClasses, 1);
  yield resetPseudoLocks(inspector, view);
}

function* testNewRule(view, pseudoClasses, index) {
  let idRuleEditor = getRuleViewRuleEditor(view, index);
  let editor = idRuleEditor.selectorText.ownerDocument.activeElement;
  let expected = EXPECTED_SELECTOR + pseudoClasses.join("");

  is(editor.value, expected,
      "Selector editor value is as expected: " + expected);

  info("Entering the escape key");
  EventUtils.synthesizeKey("VK_ESCAPE", {});

  is(idRuleEditor.selectorText.textContent, expected,
      "Selector text value is as expected: " + expected);
}

function* setPseudoLocks(inspector, view, pseudoClasses) {
  if (pseudoClasses.length == 0) {
    return;
  }

  for (let pseudoClass of pseudoClasses) {
    switch (pseudoClass) {
      case ":hover":
        view.hoverCheckbox.click();
        yield inspector.once("rule-view-refreshed");
        break;
      case ":active":
        view.activeCheckbox.click();
        yield inspector.once("rule-view-refreshed");
        break;
      case ":focus":
        view.focusCheckbox.click();
        yield inspector.once("rule-view-refreshed");
        break;
    }
  }
}

function* resetPseudoLocks(inspector, view) {
  if (!view.hoverCheckbox.checked &&
      !view.activeCheckbox.checked &&
      !view.focusCheckbox.checked) {
    return;
  }
  if (view.hoverCheckbox.checked) {
    view.hoverCheckbox.click();
    yield inspector.once("rule-view-refreshed");
  }
  if (view.activeCheckbox.checked) {
    view.activeCheckbox.click();
    yield inspector.once("rule-view-refreshed");
  }
  if (view.focusCheckbox.checked) {
    view.focusCheckbox.click();
    yield inspector.once("rule-view-refreshed");
  }
}
