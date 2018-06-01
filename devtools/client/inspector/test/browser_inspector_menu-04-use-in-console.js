/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests "Use in Console" menu item

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";

add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  await testUseInConsole();

  async function testUseInConsole() {
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
    const jsterm = hud.jsterm;

    const jstermInput = jsterm.hud.document.querySelector(".jsterm-input-node");
    is(jstermInput.value, "temp0", "first console variable is named temp0");

    let result = await jsterm.execute();
    isnot(result.textContent.indexOf('<p id="console-var">'), -1,
          "variable temp0 references correct node");

    await selectNode("#console-var-multi", inspector);
    menuItem.click();
    await inspector.once("console-var-ready");

    is(jstermInput.value, "temp1", "second console variable is named temp1");

    result = await jsterm.execute();
    isnot(result.textContent.indexOf('<p id="console-var-multi">'), -1,
          "variable temp1 references correct node");

    hud.ui.consoleOutput.dispatchClearHistory();
  }
});
