/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that when the viewport is resized, the computed-view refreshes.

const TEST_URI = "data:text/html;charset=utf-8,<html><style>" +
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

add_task(function* () {
  yield addTab(TEST_URI);

  info("Open the responsive design mode and set its size to 500x500 to start");
  let { rdm, manager } = yield openRDM();
  yield setSize(rdm, manager, 500, 500);

  info("Open the inspector, computed-view and select the test node");
  let {inspector, view} = yield openComputedView();
  yield selectNode("div", inspector);

  info("Try shrinking the viewport and checking the applied styles");
  yield testShrink(view, inspector, rdm, manager);

  info("Try growing the viewport and checking the applied styles");
  yield testGrow(view, inspector, rdm, manager);

  yield closeRDM(rdm);
  yield closeToolbox();
});

function* testShrink(computedView, inspector, rdm, manager) {
  is(computedWidth(computedView), "500px", "Should show 500px initially.");

  let onRefresh = inspector.once("computed-view-refreshed");
  yield setSize(rdm, manager, 100, 100);
  yield onRefresh;

  is(computedWidth(computedView), "100px", "Should be 100px after shrinking.");
}

function* testGrow(computedView, inspector, rdm, manager) {
  let onRefresh = inspector.once("computed-view-refreshed");
  yield setSize(rdm, manager, 500, 500);
  yield onRefresh;

  is(computedWidth(computedView), "500px", "Should be 500px after growing.");
}

function computedWidth(computedView) {
  for (let prop of computedView.propertyViews) {
    if (prop.name === "width") {
      return prop.valueNode.textContent;
    }
  }
  return null;
}
