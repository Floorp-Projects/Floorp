/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that automation view shows the correct view depending on if events
 * or params exist.
 */

add_task(async function() {
  let { target, panel } = await initWebAudioEditor(AUTOMATION_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let started = once(gFront, "start-context");

  let events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  let [actors] = await events;
  let nodeIds = actors.map(actor => actor.actorID);

  let $tabbox = $("#web-audio-editor-tabs");

  // Oscillator node
  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  await waitForInspectorRender(panelWin, EVENTS);
  $tabbox.selectedIndex = 1;

  ok(isVisible($("#automation-graph-container")), "graph container should be visible");
  ok(isVisible($("#automation-content")), "automation content should be visible");
  ok(!isVisible($("#automation-no-events")), "no-events panel should not be visible");
  ok(!isVisible($("#automation-empty")), "empty panel should not be visible");

  // Gain node
  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  await waitForInspectorRender(panelWin, EVENTS);
  $tabbox.selectedIndex = 1;

  ok(!isVisible($("#automation-graph-container")), "graph container should not be visible");
  ok(isVisible($("#automation-content")), "automation content should be visible");
  ok(isVisible($("#automation-no-events")), "no-events panel should be visible");
  ok(!isVisible($("#automation-empty")), "empty panel should not be visible");

  // destination node
  click(panelWin, findGraphNode(panelWin, nodeIds[0]));
  await waitForInspectorRender(panelWin, EVENTS);
  $tabbox.selectedIndex = 1;

  ok(!isVisible($("#automation-graph-container")), "graph container should not be visible");
  ok(!isVisible($("#automation-content")), "automation content should not be visible");
  ok(!isVisible($("#automation-no-events")), "no-events panel should not be visible");
  ok(isVisible($("#automation-empty")), "empty panel should be visible");

  await teardown(target);
});
