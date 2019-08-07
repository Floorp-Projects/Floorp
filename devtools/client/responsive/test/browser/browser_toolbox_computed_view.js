/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that when the viewport is resized, the computed-view refreshes.

const TEST_URI =
  "data:text/html;charset=utf-8,<html><style>" +
  "div {" +
  "  width: 500px;" +
  "  height: 10px;" +
  "  background: purple;" +
  "} " +
  "@media screen and (max-width: 200px) {" +
  "  div { " +
  "    width: 100px;" +
  "  }" +
  "};" +
  "</style><div></div></html>";

addRDMTask(TEST_URI, async function({ ui, manager }) {
  info("Open the responsive design mode and set its size to 500x500 to start");
  await setViewportSize(ui, manager, 500, 500);

  info("Open the inspector, computed-view and select the test node");
  const { inspector, view } = await openComputedView();
  await selectNode("div", inspector);

  info("Try shrinking the viewport and checking the applied styles");
  await testShrink(view, inspector, ui, manager);

  info("Try growing the viewport and checking the applied styles");
  await testGrow(view, inspector, ui, manager);

  await closeToolbox();
});

async function testShrink(computedView, inspector, ui, manager) {
  is(computedWidth(computedView), "500px", "Should show 500px initially.");

  const onRefresh = inspector.once("computed-view-refreshed");
  await setViewportSize(ui, manager, 100, 100);
  await onRefresh;

  is(computedWidth(computedView), "100px", "Should be 100px after shrinking.");
}

async function testGrow(computedView, inspector, ui, manager) {
  const onRefresh = inspector.once("computed-view-refreshed");
  await setViewportSize(ui, manager, 500, 500);
  await onRefresh;

  is(computedWidth(computedView), "500px", "Should be 500px after growing.");
}

function computedWidth(computedView) {
  for (const prop of computedView.propertyViews) {
    if (prop.name === "width") {
      return prop.valueNode.textContent;
    }
  }
  return null;
}
