/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that class panel updates on markup mutations

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8,<div class='c1 c2'>");
  let {inspector, view, testActor} = yield openRuleView();

  yield selectNode("div", inspector);

  info("Open the class panel");
  view.showClassPanel();

  info("Trigger an unrelated mutation on the div (id attribute change)");
  let onMutation = view.inspector.once("markupmutation");
  yield testActor.setAttribute("div", "id", "test-id");
  yield onMutation;

  info("Check that the panel still contains the right classes");
  checkClassPanelContent(view, [
    {name: "c1", state: true},
    {name: "c2", state: true}
  ]);

  info("Trigger a class mutation on a different, unknown, node");
  onMutation = view.inspector.once("markupmutation");
  yield testActor.setAttribute("body", "class", "test-class");
  yield onMutation;

  info("Check that the panel still contains the right classes");
  checkClassPanelContent(view, [
    {name: "c1", state: true},
    {name: "c2", state: true}
  ]);

  info("Trigger a class mutation on the current node");
  onMutation = view.inspector.once("markupmutation");
  yield testActor.setAttribute("div", "class", "c3 c4");
  yield onMutation;

  info("Check that the panel now contains the new classes");
  checkClassPanelContent(view, [
    {name: "c3", state: true},
    {name: "c4", state: true}
  ]);

  info("Change the state of one of the new classes");
  yield toggleClassPanelCheckBox(view, "c4");
  checkClassPanelContent(view, [
    {name: "c3", state: true},
    {name: "c4", state: false}
  ]);

  info("Select another node");
  yield selectNode("body", inspector);

  info("Trigger a class mutation on the div");
  onMutation = view.inspector.once("markupmutation");
  yield testActor.setAttribute("div", "class", "c5 c6 c7");
  yield onMutation;

  info("Go back to the previous node and check the content of the class panel." +
       "Even if hidden, it should have refreshed when we changed the DOM");
  yield selectNode("div", inspector);
  checkClassPanelContent(view, [
    {name: "c5", state: true},
    {name: "c6", state: true},
    {name: "c7", state: true}
  ]);
});
