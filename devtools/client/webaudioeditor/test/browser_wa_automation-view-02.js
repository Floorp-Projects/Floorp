/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that automation view selects the first parameter by default and
 * switching between AudioParam rerenders the graph.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(AUTOMATION_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, AutomationView } = panelWin;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  const [actors] = await events;
  const nodeIds = actors.map(actor => actor.actorID);

  // Oscillator node
  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  await waitForInspectorRender(panelWin, EVENTS);
  click(panelWin, $("#automation-tab"));

  ok(AutomationView._selectedParamName, "frequency",
    "AutomatioView is set on 'frequency'");
  ok($(".automation-param-button[data-param='frequency']").getAttribute("selected"),
    "frequency param should be selected on load");
  ok(!$(".automation-param-button[data-param='detune']").getAttribute("selected"),
    "detune param should not be selected on load");
  ok(isVisible($("#automation-content")), "automation content should be visible");
  ok(isVisible($("#automation-graph-container")), "graph container should be visible");
  ok(!isVisible($("#automation-no-events")), "no-events panel should not be visible");

  click(panelWin, $(".automation-param-button[data-param='detune']"));
  await once(panelWin, EVENTS.UI_AUTOMATION_TAB_RENDERED);

  ok(true, "automation tab rerendered");

  ok(AutomationView._selectedParamName, "detune",
    "AutomatioView is set on 'detune'");
  ok(!$(".automation-param-button[data-param='frequency']").getAttribute("selected"),
    "frequency param should not be selected after clicking detune");
  ok($(".automation-param-button[data-param='detune']").getAttribute("selected"),
    "detune param should be selected after clicking detune");
  ok(isVisible($("#automation-content")), "automation content should be visible");
  ok(!isVisible($("#automation-graph-container")), "graph container should not be visible");
  ok(isVisible($("#automation-no-events")), "no-events panel should be visible");

  await teardown(target);
});
