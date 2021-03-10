/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIGINAL_URL = "about:home";
const OTHER_URL = "about:blank";

async function waitForUrl(url, toolbox, browserTab, win) {
  return Promise.all([
    waitUntil(
      () =>
        toolbox.target.url === url &&
        browserTab.linkedBrowser.currentURI.spec === url
    ),
    toolbox.target.once("navigate"),
    toolbox.target.client.waitForRequestsToSettle(),
    waitForAboutDebuggingRequests(win.AboutDebugging.store),
  ]);
}

// Test that ensures the remote page can go forward and back via UI buttons
add_task(async function() {
  const browserTab = await addTab(ORIGINAL_URL);

  const { document, tab, window } = await openAboutDebugging();

  // go to This Firefox and inspect the new tab
  info("Inspecting a new tab in This Firefox");
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const devToolsToolbox = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ORIGINAL_URL
  );
  const { devtoolsDocument, devtoolsWindow } = devToolsToolbox;
  const toolbox = getToolbox(devtoolsWindow);

  info("Navigating to another URL");
  const urlInput = devtoolsDocument.querySelector(".devtools-textinput");
  await synthesizeUrlKeyInput(devToolsToolbox, urlInput, OTHER_URL);
  await waitForUrl(OTHER_URL, toolbox, browserTab, window);

  info("Clicking back button");
  devtoolsDocument.querySelector(".qa-back-button").click();
  await waitForUrl(ORIGINAL_URL, toolbox, browserTab, window);

  info("Clicking the forward button");
  devtoolsDocument.querySelector(".qa-forward-button").click();
  await waitForUrl(OTHER_URL, toolbox, browserTab, window);

  ok(true, "Clicking back and forward works!");
});
