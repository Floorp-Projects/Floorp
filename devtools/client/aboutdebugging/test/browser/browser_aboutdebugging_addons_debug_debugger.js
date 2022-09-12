/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

add_task(async () => {
  const EXTENSION_NAME = "temporary-web-extension";
  const EXTENSION_ID = "test-devtools@mozilla.org";

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

  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    EXTENSION_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("jsdebugger");
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

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});
