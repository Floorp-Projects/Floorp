/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests audio param connection rendering.
 */

add_task(async function() {
  let { target, panel } = await initWebAudioEditor(CONNECT_MULTI_PARAM_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let started = once(gFront, "start-context");

  let events = Promise.all([
    getN(gFront, "create-node", 5),
    waitForGraphRendered(panelWin, 5, 2, 3)
  ]);
  reload(target);
  let [actors] = await events;
  let nodeIDs = actors.map(actor => actor.actorID);

  let [, carrier, gain, mod1, mod2] = nodeIDs;

  let edges = [
    [mod1, gain, "gain", "mod1 -> gain[gain]"],
    [mod2, carrier, "frequency", "mod2 -> carrier[frequency]"],
    [mod2, carrier, "detune", "mod2 -> carrier[detune]"]
  ];

  edges.forEach(([source, target, param, msg], i) => {
    let edge = findGraphEdge(panelWin, source, target, param);
    ok(edge.classList.contains("param-connection"), "edge is classified as a param-connection");
  });

  await teardown(target);
});
