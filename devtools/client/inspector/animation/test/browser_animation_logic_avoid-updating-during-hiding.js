/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Animation inspector should not update when hidden.
// Test for followings:
// * whether the UIs update after selecting another inspector
// * whether the UIs update after selecting another tool
// * whether the UIs update after selecting animation inspector again

add_task(async function() {
  info("Switch to 2 pane inspector to see if the animation only refreshes when visible");
  await pushPref("devtools.inspector.three-pane-enabled", false);
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Checking the UIs update after selecting another inspector");
  await selectNodeAndWaitForAnimations("head", inspector);
  inspector.sidebar.select("ruleview");
  await selectNode("div", inspector);
  is(animationInspector.state.animations.length, 0,
    "Should not update after selecting another inspector");
  await selectAnimationInspector(inspector);
  is(animationInspector.state.animations.length, 1,
    "Should update after selecting animation inspector");
  await assertCurrentTimeUpdated(animationInspector, panel, true);
  inspector.sidebar.select("ruleview");
  is(animationInspector.state.animations.length, 1,
    "Should not update after selecting another inspector again");
  await assertCurrentTimeUpdated(animationInspector, panel, false);

  info("Checking the UIs update after selecting another tool");
  await selectAnimationInspector(inspector);
  await selectNodeAndWaitForAnimations("head", inspector);
  await inspector.toolbox.selectTool("webconsole");
  await selectNode("div", inspector);
  is(animationInspector.state.animations.length, 0,
    "Should not update after selecting another tool");
  await selectAnimationInspector(inspector);
  is(animationInspector.state.animations.length, 1,
    "Should update after selecting animation inspector");
  await assertCurrentTimeUpdated(animationInspector, panel, true);
  await inspector.toolbox.selectTool("webconsole");
  is(animationInspector.state.animations.length, 1,
    "Should not update after selecting another tool again");
  await assertCurrentTimeUpdated(animationInspector, panel, false);
});

async function assertCurrentTimeUpdated(animationInspector, panel, shouldRunning) {
  let count = 0;

  const listener = () => {
    count++;
  };

  animationInspector.addAnimationsCurrentTimeListener(listener);
  await new Promise(resolve => panel.ownerGlobal.requestAnimationFrame(resolve));
  animationInspector.removeAnimationsCurrentTimeListener(listener);

  if (shouldRunning) {
    isnot(count, 0, "Should forward current time");
  } else {
    is(count, 0, "Should not forward current time");
  }
}
