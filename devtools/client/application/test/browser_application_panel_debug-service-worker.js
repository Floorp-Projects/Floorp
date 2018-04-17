/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../debugger/new/test/mochitest/helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/new/test/mochitest/helpers.js",
  this);

const TAB_URL = URL_ROOT + "service-workers/debug.html";

add_task(async function() {
  await enableApplicationPanel();

  let { panel, tab, target } = await openNewTabAndApplicationPanel(TAB_URL);
  let doc = panel.panelWin.document;

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  let container = getWorkerContainers(doc)[0];
  info("Wait until the debug link is displayed and enabled");
  await waitUntil(() =>
    container.querySelector(".js-debug-link:not(.worker__debug-link--disabled)"));

  info("Click on the debug link and wait for the new toolbox to be ready");
  let onToolboxReady = gDevTools.once("toolbox-ready");

  let debugLink = container.querySelector(".js-debug-link");
  debugLink.click();

  let serviceWorkerToolbox = await onToolboxReady;
  await serviceWorkerToolbox.selectTool("jsdebugger");
  let debuggerContext = createDebuggerContext(serviceWorkerToolbox);

  await waitForSources(debuggerContext, "debug-sw.js");
  await selectSource(debuggerContext, "debug-sw.js");
  await waitForLoadedSource(debuggerContext, "debug-sw.js");

  await addBreakpoint(debuggerContext, "debug-sw.js", 8);

  info("Reload the main tab, expect the service worker script to pause on line 8");
  tab.linkedBrowser.reload();

  await waitForPaused(debuggerContext);
  assertPausedLocation(debuggerContext);
  await resume(debuggerContext);

  const workerScript = findSource(debuggerContext, "debug-sw.js");
  await removeBreakpoint(debuggerContext, workerScript.id, 8, 0);

  info("Destroy the worker toolbox");
  await serviceWorkerToolbox.destroy();

  info("Wait until the focus goes back to the main window");
  await waitUntil(() => gBrowser.selectedBrowser === tab.linkedBrowser);

  await unregisterAllWorkers(target.client);
});
