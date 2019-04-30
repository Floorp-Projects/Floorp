/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

/**
 * Test that the split console key shortcut works on about:devtools-toolbox.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsTab, devtoolsWindow } =
    await openAboutDevtoolsToolbox(document, tab, window);

  // Select any tool that is not the Webconsole, since we will assert the split-console.
  info("Select inspector tool");
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("inspector");

  info("Press Escape and wait for the split console to be opened");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, devtoolsWindow);
  await waitUntil(() => toolbox.isSplitConsoleFocused());
  ok(true, "Split console is opened and focused");

  info("Press Escape again and wait for the split console to be closed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, devtoolsWindow);
  await waitUntil(() => !toolbox.isSplitConsoleFocused());
  ok(true, "Split console is closed and no longer focused");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
});
