/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the computed view refreshes when the rule view is updated in 3 pane mode.

const TEST_URI = "<div id='target' style='color: rgb(255, 0, 0);'>test</div>";

add_task(async function () {
  info(
    "Check whether the color as well in computed view is updated " +
      "when the rule in rule view is changed in case of 3 pane mode"
  );
  await pushPref("devtools.inspector.three-pane-enabled", true);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openComputedView();
  await selectNode("#target", inspector);

  is(
    getComputedViewPropertyValue(view, "color"),
    "rgb(255, 0, 0)",
    "The computed view shows the right color"
  );

  info("Change the value in the ruleview");
  const ruleView = inspector.getPanel("ruleview").view;
  const editor = await getValueEditor(ruleView);
  const onRuleViewChanged = ruleView.once("ruleview-changed");
  const onComputedViewRefreshed = inspector.once("computed-view-refreshed");
  editor.input.value = "rgb(0, 255, 0)";
  EventUtils.synthesizeKey("VK_RETURN", {}, ruleView.styleWindow);
  await Promise.all([onRuleViewChanged, onComputedViewRefreshed]);

  info("Check the value in the computed view");
  is(
    getComputedViewPropertyValue(view, "color"),
    "rgb(0, 255, 0)",
    "The computed value is updated when the rule in ruleview is changed"
  );
});

add_task(async function () {
  info(
    "Check that the computed view is not updated " +
      "if the rule view is changed in 2 pane mode."
  );
  await pushPref("devtools.inspector.three-pane-enabled", false);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector } = await openComputedView();
  await selectNode("#target", inspector);

  info("Select the rule view");
  const ruleView = inspector.getPanel("ruleview").view;
  const onRuleViewReady = ruleView.once("ruleview-refreshed");
  const onSidebarSelect = inspector.sidebar.once("select");
  inspector.sidebar.select("ruleview");
  await Promise.all([onSidebarSelect, onRuleViewReady]);

  info(
    "Prepare the counter which counts how many times computed view is refreshed"
  );
  let computedViewRefreshCount = 0;
  const computedViewRefreshListener = () => {
    computedViewRefreshCount += 1;
  };
  inspector.on("computed-view-refreshed", computedViewRefreshListener);

  info("Change the value in the rule view");
  const editor = await getValueEditor(ruleView);
  const onRuleViewChanged = ruleView.once("ruleview-changed");
  editor.input.value = "rgb(0, 255, 0)";
  EventUtils.synthesizeKey("VK_RETURN", {}, ruleView.styleWindow);
  await onRuleViewChanged;

  info(
    "Wait for time enough to check whether the computed value is updated or not"
  );
  await wait(1000);

  info("Check the counter");
  is(computedViewRefreshCount, 0, "The computed view is not updated");

  inspector.off("computed-view-refreshed", computedViewRefreshListener);
});

async function getValueEditor(ruleView) {
  const ruleEditor = ruleView.element.children[0]._ruleEditor;
  const propEditor = ruleEditor.rule.textProps[0].editor;
  return focusEditableField(ruleView, propEditor.valueSpan);
}
