/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

/**
 * Test tooltip of markup view on about:devtools-toolbox page.
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

  const inspector = toolbox.getPanel("inspector");
  const markupDocument = inspector.markup.doc;
  const eventBadge = markupDocument.querySelector(".inspector-badge.interactive");

  info("Check tooltip visibility after clicking on an element in the markup view");
  await checkTooltipVisibility(inspector, eventBadge, markupDocument.body);

  info("Check tooltip visibility after clicking on an element in the DevTools document");
  await checkTooltipVisibility(
    inspector, eventBadge, devtoolsDocument.querySelector(".debug-target-info"));

  info("Check tooltip visibility after clicking on an element in the root document");
  const rootDocument = devtoolsWindow.windowRoot.ownerGlobal.document;
  await checkTooltipVisibility(
    inspector, eventBadge, rootDocument.querySelector("#titlebar"));

  await removeTab(devtoolsTab);
  await Promise.all([
    waitForRequestsToSettle(window.AboutDebugging.store),
    gDevTools.once("toolbox-destroyed"),
  ]);
  await removeTab(tab);
});

async function checkTooltipVisibility(inspector, elementForShowing, elementForHiding) {
  info("Show event tooltip");
  elementForShowing.click();
  const tooltip = inspector.markup.eventDetailsTooltip;
  await tooltip.once("shown");
  is(tooltip.container.classList.contains("tooltip-visible"), true,
     "The tooltip should be shown");

  info("Hide event tooltip");
  EventUtils.synthesizeMouse(elementForHiding, 1, 1, {}, elementForHiding.ownerGlobal);
  await tooltip.once("hidden");
  is(tooltip.container.classList.contains("tooltip-visible"), false,
     "Tooltip should be hidden");

  if (inspector._updateProgress) {
    info("Need to wait for the inspector to update");
    await inspector.once("inspector-updated");
  }
}
