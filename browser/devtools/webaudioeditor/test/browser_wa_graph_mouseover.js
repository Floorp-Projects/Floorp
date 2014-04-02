/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shader editor shows the appropriate UI when opened.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(COMPLEX_CONTEXT_URL);
  let { gFront, $, $$, EVENTS, WebAudioParamView } = panel.panelWin;
  let gVars = WebAudioParamView._paramsView;

  let started = once(gFront, "start-context");

  reload(target);

  yield Promise.all([
    getN(gFront, "create-node", 8),
    getNSpread(panel.panelWin, EVENTS.UI_ADD_NODE_LIST, 8),
    waitForGraphRendered(panel.panelWin, 8, 8)
  ]);

  let $items = $$(".variables-view-scope");
  let $graphNodes = $$(".nodes > g");

  for (let $item of $items) {
    mouseOver(panel.panelWin, $(".devtools-toolbar", $item));
    // Get actorID from id of variable scope
    let id = $item.id.match(/\(([^\)]*)\)/)[1];

    // Go over all graph nodes and check only the selected one is highlighted
    for (let $node of $graphNodes) {
      let shouldBeSelected = id === $node.getAttribute("data-id");
      ok($node.classList.contains("selected") === shouldBeSelected,
        "graph node correctly " + (shouldBeSelected ? "" : "not ") + "highlighted on param view mouseover");
    }
  }

  yield teardown(panel);
  finish();
}

