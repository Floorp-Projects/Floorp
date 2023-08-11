/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests "Use in Console" menu item with conflicting binding in the web content.

const TEST_URL = `data:text/html;charset=utf-8,<!DOCTYPE html>
<p id="console-var">Paragraph for testing console variables</p>
<script>
  /* Verify that the conflicting binding on user code doesn't break the
   * functionality. */
  var $0 = "user-defined variable";
</script>`;

add_task(async function () {
  // Disable eager evaluation to avoid intermittent failures due to pending
  // requests to evaluateJSAsync.
  await pushPref("devtools.webconsole.input.eagerEvaluation", false);

  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  info("Testing 'Use in Console' menu item.");

  await selectNode("#console-var", inspector);
  const container = await getContainerForSelector("#console-var", inspector);
  const allMenuItems = openContextMenuAndGetAllItems(inspector, {
    target: container.tagLine,
  });
  const menuItem = allMenuItems.find(i => i.id === "node-menu-useinconsole");
  const onConsoleVarReady = inspector.once("console-var-ready");

  menuItem.click();

  await onConsoleVarReady;

  const hud = toolbox.getPanel("webconsole").hud;

  const getConsoleResults = () => hud.ui.outputNode.querySelectorAll(".result");

  is(hud.getInputValue(), "temp0", "first console variable is named temp0");
  hud.ui.wrapper.dispatchEvaluateExpression();

  await waitUntil(() => getConsoleResults().length === 1);
  const result = getConsoleResults()[0];
  ok(
    result.textContent.includes('<p id="console-var">'),
    "variable temp0 references correct node"
  );

  hud.ui.wrapper.dispatchClearHistory();
});
