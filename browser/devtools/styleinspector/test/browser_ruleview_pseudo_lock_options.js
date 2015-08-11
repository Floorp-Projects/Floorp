/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view pseudo lock options work properly.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
    div:hover {
      color: blue;
    }
    div:active {
      color: yellow;
    }
    div:focus {
      color: green;
    }
  </style>
  <div>test div</div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);

  yield assertPseudoPanelClosed(view);

  info("Toggle the pseudo class panel open");
  view.pseudoClassToggle.click();
  yield assertPseudoPanelOpened(view);

  info("Toggle each pseudo lock and check that the pseudo lock is added");
  yield togglePseudoClass(inspector, view.hoverCheckbox);
  yield assertPseudoAdded(inspector, view, ":hover", 3, 1);
  yield togglePseudoClass(inspector, view.hoverCheckbox);
  yield assertPseudoRemoved(inspector, view, 2);

  yield togglePseudoClass(inspector, view.activeCheckbox);
  yield assertPseudoAdded(inspector, view, ":active", 3, 1);
  yield togglePseudoClass(inspector, view.activeCheckbox);
  yield assertPseudoRemoved(inspector, view, 2);

  yield togglePseudoClass(inspector, view.focusCheckbox);
  yield assertPseudoAdded(inspector, view, ":focus", 3, 1);
  yield togglePseudoClass(inspector, view.focusCheckbox);
  yield assertPseudoRemoved(inspector, view, 2);

  info("Toggle all pseudo lock and check that the pseudo lock is added");
  yield togglePseudoClass(inspector, view.hoverCheckbox);
  yield togglePseudoClass(inspector, view.activeCheckbox);
  yield togglePseudoClass(inspector, view.focusCheckbox);
  yield assertPseudoAdded(inspector, view, ":focus", 5, 1);
  yield assertPseudoAdded(inspector, view, ":active", 5, 2);
  yield assertPseudoAdded(inspector, view, ":hover", 5, 3);
  yield togglePseudoClass(inspector, view.hoverCheckbox);
  yield togglePseudoClass(inspector, view.activeCheckbox);
  yield togglePseudoClass(inspector, view.focusCheckbox);
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
  yield assertPseudoPanelClosed(view);
});

function* togglePseudoClass(inspector, pseudoClassOption) {
  info("Toggle the pseudoclass, wait for it to be applied");
  let onRefresh = inspector.once("rule-view-refreshed");
  pseudoClassOption.click();
  yield onRefresh;
}

function* assertPseudoAdded(inspector, view, pseudoClass, numRules,
    childIndex) {
  info("Check that the ruleview contains the pseudo-class rule");
  is(view.element.children.length, numRules,
    "Should have " + numRules + " rules.");
  is(getRuleViewRuleEditor(view, childIndex).rule.selectorText,
    "div" + pseudoClass, "rule view is showing " + pseudoClass + " rule");
}

function* assertPseudoRemoved(inspector, view, numRules) {
  info("Check that the ruleview no longer contains the pseudo-class rule");
  is(view.element.children.length, numRules,
    "Should have " + numRules + " rules.");
  is(getRuleViewRuleEditor(view, 1).rule.selectorText, "div",
    "Second rule is div");
}

function* assertPseudoPanelOpened(view) {
  info("Check the opened state of the pseudo class panel");

  ok(!view.pseudoClassPanel.hidden, "Pseudo Class Panel Opened");
  ok(!view.hoverCheckbox.disabled, ":hover checkbox is not disabled");
  ok(!view.activeCheckbox.disabled, ":active checkbox is not disabled");
  ok(!view.focusCheckbox.disabled, ":focus checkbox is not disabled");

  is(view.pseudoClassPanel.getAttribute("tabindex"), "-1",
    "Pseudo Class Panel has a tabindex of -1");
  is(view.hoverCheckbox.getAttribute("tabindex"), "0",
    ":hover checkbox has a tabindex of 0");
  is(view.activeCheckbox.getAttribute("tabindex"), "0",
    ":active checkbox has a tabindex of 0");
  is(view.focusCheckbox.getAttribute("tabindex"), "0",
    ":focus checkbox has a tabindex of 0");
}

function* assertPseudoPanelClosed(view) {
  info("Check the closed state of the pseudo clas panel");

  ok(view.pseudoClassPanel.hidden, "Pseudo Class Panel Hidden");

  is(view.pseudoClassPanel.getAttribute("tabindex"), "-1",
    "Pseudo Class Panel has a tabindex of -1");
  is(view.hoverCheckbox.getAttribute("tabindex"), "-1",
    ":hover checkbox has a tabindex of -1");
  is(view.activeCheckbox.getAttribute("tabindex"), "-1",
    ":active checkbox has a tabindex of -1");
  is(view.focusCheckbox.getAttribute("tabindex"), "-1",
    ":focus checkbox has a tabindex of -1");
}
