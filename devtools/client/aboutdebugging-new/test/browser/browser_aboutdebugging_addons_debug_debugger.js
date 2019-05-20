/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

add_task(async () => {
  const EXTENSION_NAME = "temporary-web-extension";
  const EXTENSION_ID = "test-devtools@mozilla.org";

  await enableExtensionDebugging();

  info("The debugger should show the source codes of extension even if " +
       "devtools.chrome.enabled and devtools.debugger.remote-enabled are off");
  await pushPref("devtools.chrome.enabled", false);
  await pushPref("devtools.debugger.remote-enabled", false);

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI({
    background: function() {
      window.someRandomMethodName = () => {
        // This will not be referred from anywhere.
        // However this is necessary to show as the source code in the debugger.
      };
    },
    id: EXTENSION_ID,
    name: EXTENSION_NAME,
  }, document);

  const { devtoolsTab, devtoolsWindow } =
    await openAboutDevtoolsToolbox(document, tab, window, EXTENSION_NAME);
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("jsdebugger");
  const { panelWin } = toolbox.getCurrentPanel();

  info("Check the state of redux");
  ok(panelWin.dbg.store.getState().debuggee.isWebExtension,
     "isWebExtension flag in debuggee is true");

  info("Check whether the element displays correctly");
  const sourceList = panelWin.document.querySelector(".sources-list");
  ok(sourceList, "Source list element displays correctly");
  ok(sourceList.textContent.includes("moz-extension"),
     "moz-extension displays correctly");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});
