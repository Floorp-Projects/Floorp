/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the ParamsList view opens the correct node when clicking
 * on the node in the GraphView
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(COMPLEX_CONTEXT_URL);
  let panelWin = panel.panelWin;
  let { gFront, $, $$, EVENTS, WebAudioParamView } = panelWin;
  let gVars = WebAudioParamView._paramsView;

  let started = once(gFront, "start-context");

  reload(target);

  let [_, nodes, _] = yield Promise.all([
    getN(gFront, "create-node", 8),
    getNSpread(panel.panelWin, EVENTS.UI_ADD_NODE_LIST, 8),
    waitForGraphRendered(panel.panelWin, 8, 8)
  ]);

  let nodeIds = nodes.map(([e, id]) => id);

  for (let i = 0; i < 8; i++) {
    ok(!isExpanded(gVars, i), "no views expanded on default");
  }

  click(panel.panelWin, findGraphNode(panelWin, nodeIds[1]));
  ok(isExpanded(gVars, 1), "params view expanded on click");

  var allClosed = true;
  for (let i = 0; i < 8; i++) {
    if (i === 1) continue;
    if (isExpanded(gVars, i))
      allClosed = false;
  }
  ok(allClosed, "all other param views are still minimized");

  click(panel.panelWin, findGraphNode(panelWin, nodeIds[2]));
  ok(isExpanded(gVars, 2), "second params view expanded on click");

  click(panel.panelWin, $("rect", findGraphNode(panelWin, nodeIds[3])));
  ok(isExpanded(gVars, 3), "param view opens when clicking `<rect>`");

  click(panel.panelWin, $("tspan", findGraphNode(panelWin, nodeIds[4])));
  ok(isExpanded(gVars, 4), "param view opens when clicking `<tspan>`");

  yield teardown(panel);
  finish();
}

function isExpanded (view, index) {
  let scope = view.getScopeAtIndex(index);
  return scope.expanded;
}
