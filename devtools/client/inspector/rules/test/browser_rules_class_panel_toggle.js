/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the class panel can be toggled.

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,<body class='class1 class2'>");
  const {inspector, view} = await openRuleView();

  info("Check that the toggle button exists");
  const button = inspector.panelDoc.querySelector("#class-panel-toggle");
  ok(button, "The class panel toggle button exists");
  is(view.classToggle, button, "The rule-view refers to the right element");

  info("Check that the panel exists and is hidden by default");
  const panel = inspector.panelDoc.querySelector("#ruleview-class-panel");
  ok(panel, "The class panel exists");
  is(view.classPanel, panel, "The rule-view refers to the right element");
  ok(panel.hasAttribute("hidden"), "The panel is hidden");

  info("Click on the button to show the panel");
  button.click();
  ok(!panel.hasAttribute("hidden"), "The panel is shown");
  ok(button.classList.contains("checked"), "The button is checked");

  info("Click again to hide the panel");
  button.click();
  ok(panel.hasAttribute("hidden"), "The panel is hidden");
  ok(!button.classList.contains("checked"), "The button is unchecked");

  info("Open the pseudo-class panel first, then the class panel");
  view.pseudoClassToggle.click();
  ok(!view.pseudoClassPanel.hasAttribute("hidden"), "The pseudo-class panel is shown");
  button.click();
  ok(!panel.hasAttribute("hidden"), "The panel is shown");
  ok(view.pseudoClassPanel.hasAttribute("hidden"), "The pseudo-class panel is hidden");

  info("Click again on the pseudo-class button");
  view.pseudoClassToggle.click();
  ok(panel.hasAttribute("hidden"), "The panel is hidden");
  ok(!view.pseudoClassPanel.hasAttribute("hidden"), "The pseudo-class panel is shown");
});
