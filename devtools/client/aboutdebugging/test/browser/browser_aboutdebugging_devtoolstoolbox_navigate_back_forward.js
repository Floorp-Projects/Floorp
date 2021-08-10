/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// FIXME(bug 1709267): This test used to test navigations between `about:home`
// and `about:blank`. Some process switch changes during Fission (bug 1650089)
// meant that this navigation now leads to a process switch. Unfortunately,
// about:debugging is not resillient to process switches, so the URLs were
// changed to both load within the same content process.
const ORIGINAL_URL = "http://example.com/document-builder.sjs?html=page1";
const OTHER_URL = "http://example.com/document-builder.sjs?html=page2";

async function waitForUrl(url, toolbox, browserTab, win) {
  const {
    onDomCompleteResource,
  } = await waitForNextTopLevelDomCompleteResource(toolbox.commands);

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
add_task(async function() {
  // Disable bfcache for Fission for now.
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

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
