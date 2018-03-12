/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that params view correctly displays non-primitive properties
 * like AudioBuffer and Float32Array in properties of AudioNodes.
 */

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(BUFFER_AND_ARRAY_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, PropertiesView } = panelWin;
  let gVars = PropertiesView._propsView;

  let started = once(gFront, "start-context");

  let events = Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  let [actors] = yield events;
  let nodeIds = actors.map(actor => actor.actorID);

  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  yield waitForInspectorRender(panelWin, EVENTS);
  checkVariableView(gVars, 0, {
    "curve": "Float32Array"
  }, "WaveShaper's `curve` is listed as an `Float32Array`.");

  let aVar = gVars.getScopeAtIndex(0).get("curve");
  let state = aVar.target.querySelector(".theme-twisty").hasAttribute("invisible");
  ok(state, "Float32Array property should not have a dropdown.");

  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  yield waitForInspectorRender(panelWin, EVENTS);
  checkVariableView(gVars, 0, {
    "buffer": "AudioBuffer"
  }, "AudioBufferSourceNode's `buffer` is listed as an `AudioBuffer`.");

  aVar = gVars.getScopeAtIndex(0).get("buffer");
  state = aVar.target.querySelector(".theme-twisty").hasAttribute("invisible");
  ok(state, "AudioBuffer property should not have a dropdown.");

  yield teardown(target);
});
