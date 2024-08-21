/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

const EXTENSION_NAME = "temporary-web-extension";
const EXTENSION_ID = "test-devtools@mozilla.org";

add_task(async function testOpenDebuggerReload() {
  await enableExtensionDebugging();

  info(
    "The debugger should show the source codes of extension even if " +
      "devtools.chrome.enabled and devtools.debugger.remote-enabled are off"
  );
  await pushPref("devtools.chrome.enabled", false);
  await pushPref("devtools.debugger.remote-enabled", false);

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background() {
        window.someRandomMethodName = () => {
          // This will not be referred from anywhere.
          // However this is necessary to show as the source code in the debugger.
        };
      },
      id: EXTENSION_ID,
      name: EXTENSION_NAME,
    },
    document
  );

  // Select the debugger right away to avoid any noise coming from the inspector.
  await pushPref("devtools.toolbox.selectedTool", "jsdebugger");
  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    EXTENSION_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  const { panelWin } = toolbox.getCurrentPanel();

  info("Check the state of redux");
  ok(
    panelWin.dbg.store.getState().sourcesTree.isWebExtension,
    "isWebExtension flag in sourcesTree is true"
  );

  info("Check whether the element displays correctly");
  let sourceList = panelWin.document.querySelector(".sources-list");
  ok(sourceList, "Source list element displays correctly");
  ok(
    sourceList.textContent.includes("temporary-web-extension"),
    "Extension name displays correctly"
  );

  const waitForLoadedPanelsReload = await watchForLoadedPanelsReload(toolbox);

  info("Reload the addon using a toolbox reload shortcut");
  toolbox.win.focus();
  synthesizeKeyShortcut(L10N.getStr("toolbox.reload.key"), toolbox.win);

  await waitForLoadedPanelsReload();

  info("Wait until a new background log message is logged");
  await waitFor(() => {
    // As React may re-create a new sources-list element,
    // fetch the latest instance
    sourceList = panelWin.document.querySelector(".sources-list");
    return sourceList?.textContent.includes("temporary-web-extension");
  }, "Wait for the source to re-appear");

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});

add_task(async function testAddAndRemoveBreakpoint() {
  await enableExtensionDebugging();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background() {
        window.invokeLogFromWebextension = () => {
          console.log("From webextension");
        };
      },
      id: EXTENSION_ID,
      name: EXTENSION_NAME,
    },
    document
  );

  // Select the debugger right away to avoid any noise coming from the inspector.
  await pushPref("devtools.toolbox.selectedTool", "jsdebugger");
  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    EXTENSION_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  const dbg = createDebuggerContext(toolbox);

  info("Select the source and add a breakpoint");
  // Note: the background script filename is dynamically generated id, so we
  // simply get the first source from the list.
  const displayedSources = dbg.selectors.getDisplayedSourcesList();
  const backgroundScript = displayedSources[0];
  await selectSource(dbg, backgroundScript);
  await addBreakpoint(dbg, backgroundScript, 3);

  info("Trigger the breakpoint and wait for the debugger to pause");
  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;
  hud.ui.wrapper.dispatchEvaluateExpression("invokeLogFromWebextension()");
  await waitForPaused(dbg);

  info("Resume and remove the breakpoint");
  await resume(dbg);
  await removeBreakpoint(dbg, backgroundScript.id, 3);

  info("Trigger the function again and check the debugger does not pause");
  hud.ui.wrapper.dispatchEvaluateExpression("invokeLogFromWebextension()");
  await wait(500);
  assertNotPaused(dbg);

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});
