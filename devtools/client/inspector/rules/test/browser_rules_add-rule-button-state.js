/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests if the `Add rule` button disables itself properly for non-element nodes
// and anonymous element.

const TEST_URI = `
  <style type="text/css">
    #pseudo::before {
      content: "before";
    }
  </style>
  <div id="pseudo"></div>
  <div id="testid">Test Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await testDisabledButton(inspector, view);
});

async function testDisabledButton(inspector, view) {
  const node = "#testid";

  info("Selecting a real element");
  await selectNode(node, inspector);
  ok(!view.addRuleButton.disabled, "Add rule button should be enabled");

  info("Select a null element");
  await view.selectElement(null);
  ok(view.addRuleButton.disabled, "Add rule button should be disabled");

  info("Selecting a real element");
  await selectNode(node, inspector);
  ok(!view.addRuleButton.disabled, "Add rule button should be enabled");

  info("Selecting a pseudo element");
  const pseudo = await getNodeFront("#pseudo", inspector);
  const children = await inspector.walker.children(pseudo);
  const before = children.nodes[0];
  await selectNode(before, inspector);
  ok(view.addRuleButton.disabled, "Add rule button should be disabled");

  info("Selecting a real element");
  await selectNode(node, inspector);
  ok(!view.addRuleButton.disabled, "Add rule button should be enabled");
}
