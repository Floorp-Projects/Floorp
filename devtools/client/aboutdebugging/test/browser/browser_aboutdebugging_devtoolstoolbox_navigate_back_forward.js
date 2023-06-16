/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIGINAL_URL = "https://example.com/document-builder.sjs?html=page1";
const OTHER_URL = "https://example.org/document-builder.sjs?html=page2";

async function waitForUrl(url, toolbox, browserTab, win) {
  const { onDomCompleteResource } =
    await waitForNextTopLevelDomCompleteResource(toolbox.commands);

  return Promise.all([
    waitUntil(
      () =>
        toolbox.target.url === url &&
        browserTab.linkedBrowser.currentURI.spec === url
    ),
    onDomCompleteResource,
    toolbox.commands.client.waitForRequestsToSettle(),
    waitForAboutDebuggingRequests(win.AboutDebugging.store),
  ]);
}

// Test that ensures the remote page can go forward and back via UI buttons
add_task(async function () {
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
  let onTargetSwitched = toolbox.commands.targetCommand.once("switched-target");
  const urlInput = devtoolsDocument.querySelector(".devtools-textinput");
  await synthesizeUrlKeyInput(devToolsToolbox, urlInput, OTHER_URL);
  await waitForUrl(OTHER_URL, toolbox, browserTab, window);
  await onTargetSwitched;

  info("Clicking back button");
  onTargetSwitched = toolbox.commands.targetCommand.once("switched-target");
  devtoolsDocument.querySelector(".qa-back-button").click();
  await waitForUrl(ORIGINAL_URL, toolbox, browserTab, window);
  await onTargetSwitched;

  info("Clicking the forward button");
  onTargetSwitched = toolbox.commands.targetCommand.once("switched-target");
  devtoolsDocument.querySelector(".qa-forward-button").click();
  await waitForUrl(OTHER_URL, toolbox, browserTab, window);
  await onTargetSwitched;

  ok(true, "Clicking back and forward works!");
});
