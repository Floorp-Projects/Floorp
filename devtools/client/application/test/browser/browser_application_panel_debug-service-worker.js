/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../debugger/test/mochitest/helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers.js",
  this
);

/* import-globals-from ../../../debugger/test/mochitest/helpers/context.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers/context.js",
  this
);

const TAB_URL = URL_ROOT + "resources/service-workers/debug.html";

add_task(async function() {
  await enableApplicationPanel();

  const { panel, tab, target } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  const container = getWorkerContainers(doc)[0];
  info("Wait until the debug button is displayed and enabled");
  await waitUntil(() => {
    const button = container.querySelector(".js-debug-button");
    return button && !button.disabled;
  });

  info("Click on the debug button and wait for the new toolbox to be ready");
  const onToolboxReady = gDevTools.once("toolbox-ready");

  const debugButton = container.querySelector(".js-debug-button");
  debugButton.click();

  const serviceWorkerToolbox = await onToolboxReady;
  await serviceWorkerToolbox.selectTool("jsdebugger");
  const debuggerContext = createDebuggerContext(serviceWorkerToolbox);

  await waitForSources(debuggerContext, "debug-sw.js");
  await selectSource(debuggerContext, "debug-sw.js");
  await waitForLoadedSource(debuggerContext, "debug-sw.js");

  await addBreakpoint(debuggerContext, "debug-sw.js", 8);

  info(
    "Reload the main tab, expect the service worker script to pause on line 8"
  );
  tab.linkedBrowser.reload();

  await waitForPaused(debuggerContext);
  assertPausedLocation(debuggerContext);
  await resume(debuggerContext);

  const workerScript = findSource(debuggerContext, "debug-sw.js");
  await removeBreakpoint(debuggerContext, workerScript.id, 8);

  info("Destroy the worker toolbox");
  await serviceWorkerToolbox.destroy();

  info("Wait until the focus goes back to the main window");
  await waitUntil(() => gBrowser.selectedBrowser === tab.linkedBrowser);

  await unregisterAllWorkers(target.client);
});
