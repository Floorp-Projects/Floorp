/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests "Use in Console" menu item

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";

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
  menuItem.click();

  await inspector.once("console-var-ready");

  const hud = toolbox.getPanel("webconsole").hud;

  const getConsoleResults = () => hud.ui.outputNode.querySelectorAll(".result");

  is(hud.getInputValue(), "temp0", "first console variable is named temp0");
  hud.ui.wrapper.dispatchEvaluateExpression();

  await waitUntil(() => getConsoleResults().length === 1);
  let result = getConsoleResults()[0];
  ok(
    result.textContent.includes('<p id="console-var">'),
    "variable temp0 references correct node"
  );

  await selectNode("#console-var-multi", inspector);
  menuItem.click();
  await inspector.once("console-var-ready");

  is(hud.getInputValue(), "temp1", "second console variable is named temp1");
  hud.ui.wrapper.dispatchEvaluateExpression();

  await waitUntil(() => getConsoleResults().length === 2);
  result = getConsoleResults()[1];
  ok(
    result.textContent.includes('<p id="console-var-multi">'),
    "variable temp1 references correct node"
  );

  hud.ui.wrapper.dispatchClearHistory();
});
