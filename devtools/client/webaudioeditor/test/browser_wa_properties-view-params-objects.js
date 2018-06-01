/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that params view correctly displays non-primitive properties
 * like AudioBuffer and Float32Array in properties of AudioNodes.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(BUFFER_AND_ARRAY_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, PropertiesView } = panelWin;
  const gVars = PropertiesView._propsView;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  const [actors] = await events;
  const nodeIds = actors.map(actor => actor.actorID);

  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  await waitForInspectorRender(panelWin, EVENTS);
  checkVariableView(gVars, 0, {
    "curve": "Float32Array"
  }, "WaveShaper's `curve` is listed as an `Float32Array`.");

  let aVar = gVars.getScopeAtIndex(0).get("curve");
  let state = aVar.target.querySelector(".theme-twisty").hasAttribute("invisible");
  ok(state, "Float32Array property should not have a dropdown.");

  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  await waitForInspectorRender(panelWin, EVENTS);
  checkVariableView(gVars, 0, {
    "buffer": "AudioBuffer"
  }, "AudioBufferSourceNode's `buffer` is listed as an `AudioBuffer`.");

  aVar = gVars.getScopeAtIndex(0).get("buffer");
  state = aVar.target.querySelector(".theme-twisty").hasAttribute("invisible");
  ok(state, "AudioBuffer property should not have a dropdown.");

  await teardown(target);
});
