/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests naviating from a page to another will repopulate
 * the audio graph if both pages have an AudioContext.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $ } = panelWin;

  let events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  await events;

  var { nodes, edges } = countGraphObjects(panelWin);
  is(nodes, 3, "should only be 3 nodes.");
  is(edges, 2, "should only be 2 edges.");

  events = Promise.all([
    getN(gFront, "create-node", 15),
    waitForGraphRendered(panelWin, 15, 0)
  ]);
  navigate(target, SIMPLE_NODES_URL);
  await events;

  is($("#reload-notice").hidden, true,
    "The 'reload this page' notice should be hidden after context found after navigation.");
  is($("#waiting-notice").hidden, true,
    "The 'waiting for an audio context' notice should be hidden after context found after navigation.");
  is($("#content").hidden, false,
    "The tool's content should reappear without closing and reopening the toolbox.");

  var { nodes, edges } = countGraphObjects(panelWin);
  is(nodes, 15, "after navigation, should have 15 nodes");
  is(edges, 0, "after navigation, should have 0 edges.");

  await teardown(target);
});
