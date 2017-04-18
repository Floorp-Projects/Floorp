/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the panel only refreshes when it is visible in the sidebar.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");

  let {inspector, panel} = yield openAnimationInspector();
  yield testRefresh(inspector, panel);
});

function* testRefresh(inspector, panel) {
  info("Select a non animated node");
  yield selectNodeAndWaitForAnimations(".still", inspector);

  info("Switch to the rule-view panel");
  inspector.sidebar.select("ruleview");

  info("Select the animated node now");
  yield selectNodeAndWaitForAnimations(".animated", inspector);

  assertAnimationsDisplayed(panel, 0,
    "The panel doesn't show the animation data while inactive");

  info("Switch to the animation panel");
  inspector.sidebar.select("animationinspector");
  yield panel.once(panel.UI_UPDATED_EVENT);

  assertAnimationsDisplayed(panel, 1,
    "The panel shows the animation data after selecting it");

  info("Switch again to the rule-view");
  inspector.sidebar.select("ruleview");

  info("Select the non animated node again");
  yield selectNode(".still", inspector);

  assertAnimationsDisplayed(panel, 1,
    "The panel still shows the previous animation data since it is inactive");

  info("Switch to the animation panel again");
  inspector.sidebar.select("animationinspector");
  yield panel.once(panel.UI_UPDATED_EVENT);

  assertAnimationsDisplayed(panel, 0,
    "The panel is now empty after refreshing");
}
