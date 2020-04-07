/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

/**
 * Test whether the focus transfers to a tab which is already inspected .
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  info(
    "Select 'performance' panel as the initial tool since the tool does not listen " +
      "any changes of the document without user action"
  );
  await pushPref("devtools.toolbox.selectedTool", "performance");

  const { document, tab, window } = await openAboutDebugging();
  const { store } = window.AboutDebugging;
  await selectThisFirefoxPage(document, store);

  const inspectionTarget = "about:debugging";
  info(`Open ${inspectionTarget} as inspection target`);
  await waitUntil(() => findDebugTargetByText(inspectionTarget, document));
  info(`Inspect ${inspectionTarget} page in about:devtools-toolbox`);
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    inspectionTarget
  );

  info(
    "Check the tab state after clicking inspect button " +
      "when another tab was selected"
  );
  const onTabsSuccess1 = waitForDispatch(store, "REQUEST_TABS_SUCCESS");
  gBrowser.selectedTab = tab;

  info("Wait for the tablist update after updating the selected tab");
  await onTabsSuccess1;

  clickInspectButton(inspectionTarget, document);
  const devtoolsURL = devtoolsWindow.location.href;
  assertDevtoolsToolboxTabState(devtoolsURL);

  info(
    "Check the tab state after clicking inspect button " +
      "when the toolbox tab is in another window"
  );
  const newNavigator = gBrowser.replaceTabWithWindow(devtoolsTab);
  await waitUntil(
    () =>
      newNavigator.gBrowser &&
      newNavigator.gBrowser.selectedTab.linkedBrowser.contentWindow.location
        .href === devtoolsURL
  );

  info(
    "Create a tab in the window and select the tab " +
      "so that the about:devtools-toolbox tab loses focus"
  );
  const newTestTab = newNavigator.gBrowser.addTab(
    "data:text/html,<title>TEST_TAB</title>",
    {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    }
  );
  await waitUntil(() => findDebugTargetByText("TEST_TAB", document));

  const onTabsSuccess2 = waitForDispatch(store, "REQUEST_TABS_SUCCESS");
  newNavigator.gBrowser.selectedTab = newTestTab;

  info("Wait for the tablist update after updating the selected tab");
  await onTabsSuccess2;

  clickInspectButton(inspectionTarget, document);
  assertDevtoolsToolboxTabState(devtoolsURL);

  info("Close new navigator and wait until the debug target disappears");
  const onToolboxDestroyed = gDevTools.once("toolbox-destroyed");
  newNavigator.close();
  await onToolboxDestroyed;
  await waitUntil(() => !findDebugTargetByText("Toolbox - ", document));

  await removeTab(tab);
});

function clickInspectButton(inspectionTarget, doc) {
  const target = findDebugTargetByText(inspectionTarget, doc);
  const button = target.querySelector(".qa-debug-target-inspect-button");
  button.click();
}

// Check that only one tab is currently opened for the provided URL.
// Also check that this tab and the tab's window are focused.
function assertDevtoolsToolboxTabState(devtoolsURL) {
  const existingTabs = [];

  for (const navigator of Services.wm.getEnumerator("navigator:browser")) {
    for (const browser of navigator.gBrowser.browsers) {
      if (
        browser.contentWindow &&
        browser.contentWindow.location.href === devtoolsURL
      ) {
        const tab = navigator.gBrowser.getTabForBrowser(browser);
        existingTabs.push(tab);
      }
    }
  }

  is(existingTabs.length, 1, `Only one tab is opened for ${devtoolsURL}`);
  const tab = existingTabs[0];
  const navigator = tab.ownerGlobal;
  is(navigator.gBrowser.selectedTab, tab, "The tab is selected");
  const focusedNavigator = Services.wm.getMostRecentWindow("navigator:browser");
  is(navigator, focusedNavigator, "The navigator is focused");
}
