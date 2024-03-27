/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests "Use in Console" menu item

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";

add_task(async function () {
  // Disable eager evaluation to avoid intermittent failures due to pending
  // requests to evaluateJSAsync.
  await pushPref("devtools.webconsole.input.eagerEvaluation", false);

  info("Testing 'Use in Console' menu item with enabled split console.");
  await pushPref("devtools.toolbox.splitconsole.enabled", true);
  await testConsoleFunctionality({ isSplitConsoleEnabled: true });

  info("Testing 'Use in Console' menu item with disabled split console.");
  await pushPref("devtools.toolbox.splitconsole.enabled", false);
  await testConsoleFunctionality({ isSplitConsoleEnabled: false });
});

async function testConsoleFunctionality({ isSplitConsoleEnabled }) {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  await selectNode("#console-var", inspector);
  const container = await getContainerForSelector("#console-var", inspector);
  const allMenuItems = openContextMenuAndGetAllItems(inspector, {
    target: container.tagLine,
  });
  const menuItem = allMenuItems.find(i => i.id === "node-menu-useinconsole");
  menuItem.click();

  await inspector.once("console-var-ready");

  const hud = toolbox.getPanel("webconsole").hud;

  if (isSplitConsoleEnabled) {
    ok(toolbox.splitConsole, "The console is split console.");
  } else {
    ok(!toolbox.splitConsole, "The console is Web Console tab.");
  }

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
}
