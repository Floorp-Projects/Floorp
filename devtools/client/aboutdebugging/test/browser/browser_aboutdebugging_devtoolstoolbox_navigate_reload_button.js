/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function clickReload(devtoolsDocument) {
  devtoolsDocument.querySelector(".qa-reload-button").click();
}

// Test that ensures the remote page is reloaded when the button is clicked
add_task(async function() {
  await addTab("about:home");

  const { document, tab, window } = await openAboutDebugging();

  // go to This Firefox and inspect the new tab
  info("Inspecting a new tab in This Firefox");
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsDocument, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    "about:home"
  );

  info("Clicking reload button and waiting for requests to complete");
  const toolbox = getToolbox(devtoolsWindow);
  const refreshes = Promise.all([
    toolbox.target.once("navigate"),
    toolbox.target.client.waitForRequestsToSettle(),
    waitForAboutDebuggingRequests(window.AboutDebugging.store),
  ]);
  clickReload(devtoolsDocument);
  await refreshes;

  ok(true, "Clicked refresh; both page and devtools reloaded");
});
