/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that params view shows params when they exist, and are hidden otherwise.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, PropertiesView } = panelWin;
  const gVars = PropertiesView._propsView;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  const [actors] = await events;
  const nodeIds = actors.map(actor => actor.actorID);

  // Gain node
  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  await waitForInspectorRender(panelWin, EVENTS);

  ok(isVisible($("#properties-content")), "Parameters shown when they exist.");
  ok(!isVisible($("#properties-empty")),
    "Empty message hidden when AudioParams exist.");

  // Destination node
  click(panelWin, findGraphNode(panelWin, nodeIds[0]));
  await waitForInspectorRender(panelWin, EVENTS);

  ok(!isVisible($("#properties-content")),
    "Parameters hidden when they don't exist.");
  ok(isVisible($("#properties-empty")),
    "Empty message shown when no AudioParams exist.");

  await teardown(target);
});
