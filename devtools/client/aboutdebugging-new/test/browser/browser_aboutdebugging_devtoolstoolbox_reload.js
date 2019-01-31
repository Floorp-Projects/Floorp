/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

/**
 * Test whether about:devtools-toolbox display correctly after reloading.
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

  info("Select webconsole tool");
  const devtoolsBrowser = gBrowser.selectedBrowser;
  const toolbox = getToolbox(devtoolsBrowser.contentWindow);
  await toolbox.selectTool("webconsole");

  info("Reload about:devtools-toolbox page");
  devtoolsBrowser.reload();
  await gDevTools.once("toolbox-ready");
  ok(true, "Toolbox is re-created again");

  info("Check whether about:devtools-toolbox page displays correctly");
  ok(devtoolsBrowser.contentDocument.querySelector(".debug-target-info"),
     "about:devtools-toolbox page displays correctly");

  await removeTab(devtoolsTab);
  await Promise.all([
    waitForRequestsToSettle(window.AboutDebugging.store),
    gDevTools.once("toolbox-destroyed"),
  ]);
  await removeTab(tab);
});
