/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

/**
 * Test context menu of markup view on about:devtools-toolbox page.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsTab, devtoolsWindow } =
    await openAboutDevtoolsToolbox(document, tab, window);

  info("Select inspector tool");
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("inspector");

  info("Show context menu of markup view");
  const markupDocument = toolbox.getPanel("inspector").markup.doc;
  EventUtils.synthesizeMouseAtCenter(markupDocument.body,
                                     { type: "contextmenu" },
                                     markupDocument.ownerGlobal);

  info("Check whether proper context menu of markup view will be shown");
  await waitUntil(() => toolbox.topDoc.querySelector("#node-menu-edithtml"));
  ok(true, "Context menu of markup view should be shown");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
});
