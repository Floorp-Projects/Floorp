/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that SVG nodes and edges were created for the Graph View.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS } = panelWin;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  const [actors] = await events;
  const [destId, oscId, gainId] = actors.map(actor => actor.actorID);

  ok(!findGraphNode(panelWin, destId).classList.contains("selected"),
    "No nodes selected on start. (destination)");
  ok(!findGraphNode(panelWin, oscId).classList.contains("selected"),
    "No nodes selected on start. (oscillator)");
  ok(!findGraphNode(panelWin, gainId).classList.contains("selected"),
    "No nodes selected on start. (gain)");

  await clickGraphNode(panelWin, oscId);

  ok(findGraphNode(panelWin, oscId).classList.contains("selected"),
    "Selected node has class 'selected'.");
  ok(!findGraphNode(panelWin, destId).classList.contains("selected"),
    "Non-selected nodes do not have class 'selected'.");
  ok(!findGraphNode(panelWin, gainId).classList.contains("selected"),
    "Non-selected nodes do not have class 'selected'.");

  await clickGraphNode(panelWin, gainId);

  ok(!findGraphNode(panelWin, oscId).classList.contains("selected"),
    "Previously selected node no longer has class 'selected'.");
  ok(!findGraphNode(panelWin, destId).classList.contains("selected"),
    "Non-selected nodes do not have class 'selected'.");
  ok(findGraphNode(panelWin, gainId).classList.contains("selected"),
    "Newly selected node now has class 'selected'.");

  await teardown(target);
});
