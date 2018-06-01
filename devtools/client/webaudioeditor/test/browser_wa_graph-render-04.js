/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests audio param connection rendering.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(CONNECT_MULTI_PARAM_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS } = panelWin;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    getN(gFront, "create-node", 5),
    waitForGraphRendered(panelWin, 5, 2, 3)
  ]);
  reload(target);
  const [actors] = await events;
  const nodeIDs = actors.map(actor => actor.actorID);

  const [, carrier, gain, mod1, mod2] = nodeIDs;

  const edges = [
    [mod1, gain, "gain", "mod1 -> gain[gain]"],
    [mod2, carrier, "frequency", "mod2 -> carrier[frequency]"],
    [mod2, carrier, "detune", "mod2 -> carrier[detune]"]
  ];

  edges.forEach(([source, target, param, msg], i) => {
    const edge = findGraphEdge(panelWin, source, target, param);
    ok(edge.classList.contains("param-connection"), "edge is classified as a param-connection");
  });

  await teardown(target);
});
