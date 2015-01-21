/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that automation view shows the correct view depending on if events
 * or params exist.
 */

add_task(function*() {
  let { target, panel } = yield initWebAudioEditor(AUTOMATION_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let started = once(gFront, "start-context");

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  let nodeIds = actors.map(actor => actor.actorID);
  let $tabbox = $("#web-audio-editor-tabs");

  // Oscillator node
  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  yield waitForInspectorRender(panelWin, EVENTS);
  $tabbox.selectedIndex = 1;

  ok(isVisible($("#automation-graph-container")), "graph container should be visible");
  ok(isVisible($("#automation-content")), "automation content should be visible");
  ok(!isVisible($("#automation-no-events")), "no-events panel should not be visible");
  ok(!isVisible($("#automation-empty")), "empty panel should not be visible");

  // Gain node
  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  yield waitForInspectorRender(panelWin, EVENTS);
  $tabbox.selectedIndex = 1;

  ok(!isVisible($("#automation-graph-container")), "graph container should not be visible");
  ok(isVisible($("#automation-content")), "automation content should be visible");
  ok(isVisible($("#automation-no-events")), "no-events panel should be visible");
  ok(!isVisible($("#automation-empty")), "empty panel should not be visible");

  // destination node
  click(panelWin, findGraphNode(panelWin, nodeIds[0]));
  yield waitForInspectorRender(panelWin, EVENTS);
  $tabbox.selectedIndex = 1;

  ok(!isVisible($("#automation-graph-container")), "graph container should not be visible");
  ok(!isVisible($("#automation-content")), "automation content should not be visible");
  ok(!isVisible($("#automation-no-events")), "no-events panel should not be visible");
  ok(isVisible($("#automation-empty")), "empty panel should be visible");

  yield teardown(target);
});
