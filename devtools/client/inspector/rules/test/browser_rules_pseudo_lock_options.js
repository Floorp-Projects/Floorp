/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view pseudo lock options work properly.

const {
  PSEUDO_CLASSES,
} = require("resource://devtools/shared/css/constants.js");
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
    div:visited {
      color: orange;
    }
    div:focus-visible {
      color: wheat;
    }
    div:target {
      color: crimson;
    }
  </style>
  <div>test div</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  info("Check that the toggle button exists");
  const button = inspector.panelDoc.getElementById("pseudo-class-panel-toggle");
  ok(button, "The pseudo-class panel toggle button exists");
  is(
    view.pseudoClassToggle,
    button,
    "The rule-view refers to the right element"
  );
  is(
    inspector.panelDoc.getElementById(button.getAttribute("aria-controls")),
    view.pseudoClassPanel,
    "The pseudo-class panel toggle button has valid aria-controls attribute"
  );

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

  info("Toggle all pseudo locks and check that the pseudo lock is added");
  await togglePseudoClass(inspector, view, ":hover");
  await togglePseudoClass(inspector, view, ":active");
  await togglePseudoClass(inspector, view, ":focus");
  await togglePseudoClass(inspector, view, ":target");
  await assertPseudoAdded(inspector, view, ":target", 6, 1);
  await assertPseudoAdded(inspector, view, ":focus", 6, 2);
  await assertPseudoAdded(inspector, view, ":active", 6, 3);
  await assertPseudoAdded(inspector, view, ":hover", 6, 4);
  await togglePseudoClass(inspector, view, ":hover");
  await togglePseudoClass(inspector, view, ":active");
  await togglePseudoClass(inspector, view, ":focus");
  await togglePseudoClass(inspector, view, ":target");
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
  info(`Toggle the pseudo-class ${pseudoClass}, wait for it to be applied`);
  const onRefresh = inspector.once("rule-view-refreshed");
  const checkbox = getPseudoClassCheckbox(view, pseudoClass);
  if (checkbox) {
    checkbox.click();
  }
  await onRefresh;
}

function assertPseudoAdded(inspector, view, pseudoClass, numRules, childIndex) {
  info("Check that the rule view contains the pseudo-class rule");
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
  info("Check that the rule view no longer contains the pseudo-class rule");
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
  is(
    view.pseudoClassToggle.getAttribute("aria-pressed"),
    "true",
    "The toggle button is pressed"
  );

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
  is(
    view.pseudoClassToggle.getAttribute("aria-pressed"),
    "false",
    "The toggle button is not pressed"
  );

  for (const pseudo of PSEUDO_CLASSES) {
    const checkbox = getPseudoClassCheckbox(view, pseudo);
    is(
      checkbox.getAttribute("tabindex"),
      "-1",
      `${pseudo} checkbox has a tabindex of -1`
    );
  }
}
