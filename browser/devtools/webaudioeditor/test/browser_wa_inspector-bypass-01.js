/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that nodes are correctly bypassed when bypassing.
 */

add_task(function*() {
  let { target, panel } = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, gAudioNodes } = panelWin;

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  let nodeIds = actors.map(actor => actor.actorID);

  // Wait for the node to be set as well as the inspector to come fully into the view
  yield clickGraphNode(panelWin, findGraphNode(panelWin, nodeIds[1]), true);

  let $bypass = $("toolbarbutton.bypass");

  is((yield actors[1].isBypassed()), false, "AudioNodeActor is not bypassed by default.")
  is($bypass.checked, true, "Button is 'on' for normal nodes");
  is($bypass.disabled, false, "Bypass button is not disabled for normal nodes");

  command($bypass);
  yield once(gAudioNodes, "bypass");

  is((yield actors[1].isBypassed()), true, "AudioNodeActor is bypassed.")
  is($bypass.checked, false, "Button is 'off' when clicked");
  is($bypass.disabled, false, "Bypass button is not disabled after click");
  ok(findGraphNode(panelWin, nodeIds[1]).classList.contains("bypassed"),
    "AudioNode has 'bypassed' class.");

  command($bypass);
  yield once(gAudioNodes, "bypass");

  is((yield actors[1].isBypassed()), false, "AudioNodeActor is no longer bypassed.")
  is($bypass.checked, true, "Button is back on when clicked");
  is($bypass.disabled, false, "Bypass button is not disabled after click");
  ok(!findGraphNode(panelWin, nodeIds[1]).classList.contains("bypassed"),
    "AudioNode no longer has 'bypassed' class.");

  yield clickGraphNode(panelWin, findGraphNode(panelWin, nodeIds[0]));

  is((yield actors[0].isBypassed()), false, "Unbypassable AudioNodeActor is not bypassed.");
  is($bypass.checked, false, "Button is 'off' for unbypassable nodes");
  is($bypass.disabled, true, "Bypass button is disabled for unbypassable nodes");

  command($bypass);
  is((yield actors[0].isBypassed()), false,
    "Clicking button on unbypassable node does not change bypass state on actor.");
  is($bypass.checked, false, "Button is still 'off' for unbypassable nodes");
  is($bypass.disabled, true, "Bypass button is still disabled for unbypassable nodes");

  yield teardown(target);
});
