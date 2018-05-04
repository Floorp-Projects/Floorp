/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the panel only refreshes when it is visible in the sidebar.

add_task(async function() {
  info("Switch to 2 pane inspector to see if the panel only refreshes when visible");
  await pushPref("devtools.inspector.three-pane-enabled", false);
  await addTab(URL_ROOT + "doc_simple_animation.html");

  let {inspector, panel} = await openAnimationInspector();
  await testRefresh(inspector, panel);
});

async function testRefresh(inspector, panel) {
  info("Select a non animated node");
  await selectNodeAndWaitForAnimations(".still", inspector);

  info("Switch to the rule-view panel");
  inspector.sidebar.select("ruleview");

  info("Select the animated node now");
  await selectNode(".animated", inspector);

  assertAnimationsDisplayed(panel, 0,
    "The panel doesn't show the animation data while inactive");

  info("Switch to the animation panel");
  let onRendered = waitForAnimationTimelineRendering(panel);
  inspector.sidebar.select("animationinspector");
  await panel.once(panel.UI_UPDATED_EVENT);
  await onRendered;

  assertAnimationsDisplayed(panel, 1,
    "The panel shows the animation data after selecting it");

  info("Switch again to the rule-view");
  inspector.sidebar.select("ruleview");

  info("Select the non animated node again");
  await selectNode(".still", inspector);

  assertAnimationsDisplayed(panel, 1,
    "The panel still shows the previous animation data since it is inactive");

  info("Switch to the animation panel again");
  inspector.sidebar.select("animationinspector");
  await panel.once(panel.UI_UPDATED_EVENT);

  assertAnimationsDisplayed(panel, 0,
    "The panel is now empty after refreshing");
}
