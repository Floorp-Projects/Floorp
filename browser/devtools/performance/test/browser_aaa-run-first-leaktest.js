/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the performance tool leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 */

function* spawnTest() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);

  ok(target, "Should have a target available.");
  ok(toolbox, "Should have a toolbox available.");
  ok(panel, "Should have a panel available.");

  ok(panel.panelWin.gToolbox, "Should have a toolbox reference on the panel window.");
  ok(panel.panelWin.gTarget, "Should have a target reference on the panel window.");
  ok(panel.panelWin.gFront, "Should have a front reference on the panel window.");

  yield teardown(panel);
  finish();
}
