/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that class panel updates on markup mutations

add_task(async function() {
  const tab = await addTab("data:text/html;charset=utf-8,<div class='c1 c2'>");
  const browser = tab.linkedBrowser;
  const { inspector, view } = await openRuleView();

  await selectNode("div", inspector);

  info("Open the class panel");
  view.showClassPanel();

  info("Trigger an unrelated mutation on the div (id attribute change)");
  let onMutation = view.inspector.once("markupmutation");
  await setAttributeInBrowser(browser, "div", "id", "test-id");
  await onMutation;

  info("Check that the panel still contains the right classes");
  checkClassPanelContent(view, [
    { name: "c1", state: true },
    { name: "c2", state: true },
  ]);

  info("Trigger a class mutation on a different, unknown, node");
  onMutation = view.inspector.once("markupmutation");
  await setAttributeInBrowser(browser, "body", "class", "test-class");
  await onMutation;

  info("Check that the panel still contains the right classes");
  checkClassPanelContent(view, [
    { name: "c1", state: true },
    { name: "c2", state: true },
  ]);

  info("Trigger a class mutation on the current node");
  onMutation = view.inspector.once("markupmutation");
  await setAttributeInBrowser(browser, "div", "class", "c3 c4");
  await onMutation;

  info("Check that the panel now contains the new classes");
  checkClassPanelContent(view, [
    { name: "c3", state: true },
    { name: "c4", state: true },
  ]);

  info("Change the state of one of the new classes");
  await toggleClassPanelCheckBox(view, "c4");
  checkClassPanelContent(view, [
    { name: "c3", state: true },
    { name: "c4", state: false },
  ]);

  info("Select another node");
  await selectNode("body", inspector);

  info("Trigger a class mutation on the div");
  onMutation = view.inspector.once("markupmutation");
  await setAttributeInBrowser(browser, "div", "class", "c5 c6 c7");
  await onMutation;

  info(
    "Go back to the previous node and check the content of the class panel." +
      "Even if hidden, it should have refreshed when we changed the DOM"
  );
  await selectNode("div", inspector);
  checkClassPanelContent(view, [
    { name: "c5", state: true },
    { name: "c6", state: true },
    { name: "c7", state: true },
  ]);
});
