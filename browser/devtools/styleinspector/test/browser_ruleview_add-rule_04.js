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

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield testDisabledButton(inspector, view);
});

function* testDisabledButton(inspector, view) {
  let node = "#testid";

  info("Selecting a real element");
  yield selectNode(node, inspector);
  ok(!view.addRuleButton.disabled, "Add rule button should be enabled");

  info("Select a null element");
  yield view.selectElement(null);
  ok(view.addRuleButton.disabled, "Add rule button should be disabled");

  info("Selecting a real element");
  yield selectNode(node, inspector);
  ok(!view.addRuleButton.disabled, "Add rule button should be enabled");

  info("Selecting a pseudo element");
  let pseudo = yield getNodeFront("#pseudo", inspector);
  let children = yield inspector.walker.children(pseudo);
  let before = children.nodes[0];
  yield selectNode(before, inspector);
  ok(view.addRuleButton.disabled, "Add rule button should be disabled");

  info("Selecting a real element");
  yield selectNode(node, inspector);
  ok(!view.addRuleButton.disabled, "Add rule button should be enabled");
}
