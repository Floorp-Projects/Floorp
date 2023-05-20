/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function clickReload(devtoolsDocument) {
  devtoolsDocument.querySelector(".qa-reload-button").click();
}

// Test that ensures the remote page is reloaded when the button is clicked
add_task(async function () {
  const debug_tab = await addTab("about:home");

  const { document, tab, window } = await openAboutDebugging();

  // go to This Firefox and inspect the new tab
  info("Inspecting a new tab in This Firefox");
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsDocument, devtoolsTab, devtoolsWindow } =
    await openAboutDevtoolsToolbox(document, tab, window, "about:home");

  info("Clicking reload button and waiting for requests to complete");
  const toolbox = getToolbox(devtoolsWindow);
  const { onDomCompleteResource } =
    await waitForNextTopLevelDomCompleteResource(toolbox.commands);

  // Watch for navigation promises.
  const refreshes = Promise.all([
    onDomCompleteResource,
    toolbox.commands.client.waitForRequestsToSettle(),
    waitForAboutDebuggingRequests(window.AboutDebugging.store),
  ]);

  // We cannot include this one in the Promise.all array, as it needs to be
  // explicitly called after navigation started.
  const waitForLoadedPanelsReload = await watchForLoadedPanelsReload(toolbox);

  clickReload(devtoolsDocument);
  await refreshes;
  await waitForLoadedPanelsReload();

  ok(true, "Clicked refresh; both page and devtools reloaded");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  info("Remove the debugged tab");
  await removeTab(debug_tab);
  await waitUntil(() => !findDebugTargetByText("about:home", document));
  await waitForAboutDebuggingRequests(window.AboutDebugging.store);

  info("Remove the about:debugging tab.");
  await removeTab(tab);
});
