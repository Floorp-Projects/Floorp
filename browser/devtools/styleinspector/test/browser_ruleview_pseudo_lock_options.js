/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view pseudo lock options work properly.

let TEST_URI = [
  "<style type='text/css'>",
  "  div {",
  "    color: red;",
  "  }",
  "  div:hover {",
  "    color: blue;",
  "  }",
  "  div:active {",
  "    color: yellow;",
  "  }",
  "  div:focus {",
  "    color: green;",
  "  }",
  "</style>",
  "<div>test div</div>"
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);

  info("Toggle the pseudo class panel open");
  ok(view.pseudoClassPanel.hidden, "Pseudo Class Panel Hidden");
  view.pseudoClassToggle.click();
  ok(!view.pseudoClassPanel.hidden, "Pseudo Class Panel Opened");
  ok(!view.hoverCheckbox.disabled, ":hover checkbox is not disabled");
  ok(!view.activeCheckbox.disabled, ":active checkbox is not disabled");
  ok(!view.focusCheckbox.disabled, ":focus checkbox is not disabled");

  info("Toggle each pseudo lock and check that the pseudo lock is added");
  yield togglePseudoClass(inspector, view, view.hoverCheckbox);
  yield assertPseudoAdded(inspector, view, ":hover", 3, 1);
  yield togglePseudoClass(inspector, view, view.hoverCheckbox);
  yield assertPseudoRemoved(inspector, view, 2);

  yield togglePseudoClass(inspector, view, view.activeCheckbox);
  yield assertPseudoAdded(inspector, view, ":active", 3, 1);
  yield togglePseudoClass(inspector, view, view.activeCheckbox);
  yield assertPseudoRemoved(inspector, view, 2);

  yield togglePseudoClass(inspector, view, view.focusCheckbox);
  yield assertPseudoAdded(inspector, view, ":focus", 3, 1);
  yield togglePseudoClass(inspector, view, view.focusCheckbox);
  yield assertPseudoRemoved(inspector, view, 2);

  info("Toggle all pseudo lock and check that the pseudo lock is added");
  yield togglePseudoClass(inspector, view, view.hoverCheckbox);
  yield togglePseudoClass(inspector, view, view.activeCheckbox);
  yield togglePseudoClass(inspector, view, view.focusCheckbox);
  yield assertPseudoAdded(inspector, view, ":focus", 5, 1);
  yield assertPseudoAdded(inspector, view, ":active", 5, 2);
  yield assertPseudoAdded(inspector, view, ":hover", 5, 3);
  yield togglePseudoClass(inspector, view, view.hoverCheckbox);
  yield togglePseudoClass(inspector, view, view.activeCheckbox);
  yield togglePseudoClass(inspector, view, view.focusCheckbox);
  yield assertPseudoRemoved(inspector, view, 2);

  info("Select a null element");
  yield view.selectElement(null);
  ok(!view.hoverCheckbox.checked && view.hoverCheckbox.disabled,
    ":hover checkbox is unchecked and disabled");
  ok(!view.activeCheckbox.checked && view.activeCheckbox.disabled,
    ":active checkbox is unchecked and disabled");
  ok(!view.focusCheckbox.checked && view.focusCheckbox.disabled,
    ":focus checkbox is unchecked and disabled");

  info("Toggle the pseudo class panel close");
  view.pseudoClassToggle.click();
  ok(view.pseudoClassPanel.hidden, "Pseudo Class Panel Closed");
});

function* togglePseudoClass(inspector, ruleView, pseudoClassOption) {
  info("Toggle the pseudoclass, wait for it to be applied");
  let onRefresh = inspector.once("rule-view-refreshed");
  pseudoClassOption.click();
  yield onRefresh;
}

function* assertPseudoAdded(inspector, ruleView, pseudoClass, numRules,
    childIndex) {
  info("Check that the ruleview contains the pseudo-class rule");
  is(ruleView.element.children.length, numRules,
    "Should have " + numRules + " rules.");
  is(getRuleViewRuleEditor(ruleView, childIndex).rule.selectorText,
    "div" + pseudoClass, "rule view is showing " + pseudoClass + " rule");
}

function* assertPseudoRemoved(inspector, ruleView, numRules) {
  info("Check that the ruleview no longer contains the pseudo-class rule");
  is(ruleView.element.children.length, numRules,
    "Should have " + numRules + " rules.");
  is(getRuleViewRuleEditor(ruleView, 1).rule.selectorText, "div",
    "Second rule is div");
}
