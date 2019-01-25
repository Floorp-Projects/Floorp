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

  info("Show about:devtools-toolbox page");
  const target = findDebugTargetByText("about:debugging", document);
  ok(target, "about:debugging tab target appeared");
  const inspectButton = target.querySelector(".js-debug-target-inspect-button");
  ok(inspectButton, "Inspect button for about:debugging appeared");
  inspectButton.click();
  await Promise.all([
    waitUntil(() => tab.nextElementSibling),
    waitForRequestsToSettle(window.AboutDebugging.store),
    gDevTools.once("toolbox-ready"),
  ]);

  info("Wait for about:devtools-toolbox tab will be selected");
  const devtoolsTab = tab.nextElementSibling;
  await waitUntil(() => gBrowser.selectedTab === devtoolsTab);

  info("Select inspector tool");
  const devtoolsBrowser = gBrowser.selectedBrowser;
  const devtoolsDocument = devtoolsBrowser.contentDocument;
  const devtoolsWindow = devtoolsBrowser.contentWindow;
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("inspector");

  info("Show context menu of markup view");
  const markupFrame = getMarkupViewFrame(devtoolsDocument);
  const markupDocument = markupFrame.contentDocument;
  const markupWindow = markupFrame.contentWindow;
  EventUtils.synthesizeMouseAtCenter(markupDocument.body,
                                     { type: "contextmenu" },
                                     markupWindow);

  info("Check whether proper context menu of markup view will be shown");
  await waitUntil(() => devtoolsDocument.querySelector("#node-menu-edithtml"));
  ok(true, "Context menu of markup view should be shown");

  await removeTab(devtoolsTab);
  await Promise.all([
    waitForRequestsToSettle(window.AboutDebugging.store),
    gDevTools.once("toolbox-destroyed"),
  ]);
  await removeTab(tab);
});

function getMarkupViewFrame(rootDocument) {
  const inspectorFrame = rootDocument.querySelector("#toolbox-panel-iframe-inspector");
  return inspectorFrame.contentDocument.querySelector("#markup-box iframe");
}

function getToolbox(win) {
  return gDevTools.getToolboxes().find(toolbox => toolbox.win === win);
}
