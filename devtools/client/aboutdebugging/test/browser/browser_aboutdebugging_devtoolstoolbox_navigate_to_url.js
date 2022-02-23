/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NEW_TAB_TITLE = "PAGE 2";
const TAB_URL = "data:text/html,<title>PAGE</title>";
const NEW_TAB_URL = `data:text/html,<title>${NEW_TAB_TITLE}</title>`;

/**
 * This test file ensures that the URL input for DebugTargetInfo navigates the target to
 * the specified URL.
 */
add_task(async function() {
  const { document, tab, window } = await openAboutDebugging();

  info("Open a new background tab.");
  const debug_tab = await addTab(TAB_URL, { background: true });

  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const devToolsToolbox = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    "PAGE"
  );
  const { devtoolsDocument, devtoolsTab, devtoolsWindow } = devToolsToolbox;
  const toolbox = getToolbox(devtoolsWindow);

  const urlInput = devtoolsDocument.querySelector(".devtools-textinput");
  const waitForLoadedPanelsReload = await watchForLoadedPanelsReload(toolbox);

  await synthesizeUrlKeyInput(devToolsToolbox, urlInput, NEW_TAB_URL);

  await waitForLoadedPanelsReload();

  info("Test that the debug target navigated to the specified URL.");
  await waitUntil(
    () =>
      toolbox.target.url === NEW_TAB_URL &&
      debug_tab.linkedBrowser.currentURI.spec === NEW_TAB_URL
  );
  ok(true, "Target navigated.");
  ok(toolbox.target.title.includes(NEW_TAB_TITLE), "Target's title updated.");
  is(urlInput.value, NEW_TAB_URL, "Input url updated.");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  info("Remove the background tab");
  await removeTab(debug_tab);
  await waitUntil(() => !findDebugTargetByText("NEW_TAB_TITLE", document));
  await waitForAboutDebuggingRequests(window.AboutDebugging.store);

  info("Remove the about:debugging tab.");
  await removeTab(tab);
});
