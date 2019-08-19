/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view pseudo lock options work properly.

const { PSEUDO_CLASSES } = require("devtools/shared/css/constants");
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
    div:focus-within {
      color: papayawhip;
    }
  </style>
  <div>test div</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  await assertPseudoPanelClosed(view);

  info("Toggle the pseudo class panel open");
  view.pseudoClassToggle.click();
  await assertPseudoPanelOpened(view);

  info("Toggle each pseudo lock and check that the pseudo lock is added");
  for (const pseudo of PSEUDO_CLASSES) {
    await togglePseudoClass(inspector, view, pseudo);
    await assertPseudoAdded(inspector, view, pseudo, 3, 1);
    await togglePseudoClass(inspector, view, pseudo);
    await assertPseudoRemoved(inspector, view, 2);
  }

  info("Toggle all pseudo lock and check that the pseudo lock is added");
  await togglePseudoClass(inspector, view, ":hover");
  await togglePseudoClass(inspector, view, ":active");
  await togglePseudoClass(inspector, view, ":focus");
  await assertPseudoAdded(inspector, view, ":focus", 5, 1);
  await assertPseudoAdded(inspector, view, ":active", 5, 2);
  await assertPseudoAdded(inspector, view, ":hover", 5, 3);
  await togglePseudoClass(inspector, view, ":hover");
  await togglePseudoClass(inspector, view, ":active");
  await togglePseudoClass(inspector, view, ":focus");
  await assertPseudoRemoved(inspector, view, 2);

  info("Select a null element");
  await view.selectElement(null);

  info("Check that all pseudo locks are unchecked and disabled");
  for (const pseudo of PSEUDO_CLASSES) {
    const checkbox = getPseudoClassCheckbox(view, pseudo);
    ok(
      !checkbox.checked && checkbox.disabled,
      `${pseudo} checkbox is unchecked and disabled`
    );
  }

  info("Toggle the pseudo class panel close");
  view.pseudoClassToggle.click();
  await assertPseudoPanelClosed(view);
});

async function togglePseudoClass(inspector, view, pseudoClass) {
  info("Toggle the pseudoclass, wait for it to be applied");
  const onRefresh = inspector.once("rule-view-refreshed");
  const checkbox = getPseudoClassCheckbox(view, pseudoClass);
  if (checkbox) {
    checkbox.click();
  }
  await onRefresh;
}

function assertPseudoAdded(inspector, view, pseudoClass, numRules, childIndex) {
  info("Check that the ruleview contains the pseudo-class rule");
  is(
    view.element.children.length,
    numRules,
    "Should have " + numRules + " rules."
  );
  is(
    getRuleViewRuleEditor(view, childIndex).rule.selectorText,
    "div" + pseudoClass,
    "rule view is showing " + pseudoClass + " rule"
  );
}

function assertPseudoRemoved(inspector, view, numRules) {
  info("Check that the ruleview no longer contains the pseudo-class rule");
  is(
    view.element.children.length,
    numRules,
    "Should have " + numRules + " rules."
  );
  is(
    getRuleViewRuleEditor(view, 1).rule.selectorText,
    "div",
    "Second rule is div"
  );
}

function assertPseudoPanelOpened(view) {
  info("Check the opened state of the pseudo class panel");

  ok(!view.pseudoClassPanel.hidden, "Pseudo Class Panel Opened");

  for (const pseudo of PSEUDO_CLASSES) {
    const checkbox = getPseudoClassCheckbox(view, pseudo);
    ok(!checkbox.disabled, `${pseudo} checkbox is not disabled`);
    is(
      checkbox.getAttribute("tabindex"),
      "0",
      `${pseudo} checkbox has a tabindex of 0`
    );
  }
}

function assertPseudoPanelClosed(view) {
  info("Check the closed state of the pseudo clas panel");

  ok(view.pseudoClassPanel.hidden, "Pseudo Class Panel Hidden");

  for (const pseudo of PSEUDO_CLASSES) {
    const checkbox = getPseudoClassCheckbox(view, pseudo);
    is(
      checkbox.getAttribute("tabindex"),
      "-1",
      `${pseudo} checkbox has a tabindex of -1`
    );
  }
}
