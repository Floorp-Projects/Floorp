/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

const ADDON_NOBG_ID = "test-devtools-webextension-nobg@mozilla.org";
const ADDON_NOBG_NAME = "test-devtools-webextension-nobg";

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - the webextension developer toolbox is connected to a fallback page when the
 *   background page is not available (and in the fallback page document body contains
 *   the expected message, which warns the user that the current page is not a real
 *   webextension context);
 */
add_task(async function testWebExtensionsToolboxNoBackgroundPage() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  const store = window.AboutDebugging.store;
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      // Do not pass any `background` script.
      id: ADDON_NOBG_ID,
      name: ADDON_NOBG_NAME,
    },
    document
  );

  info("Open a toolbox to debug the addon");
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NOBG_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);

  const onToolboxClose = gDevTools.once("toolbox-destroyed");
  toolboxTestScript(toolbox, devtoolsTab, window);

  // we need to wait for the tabs request to complete before continuing
  await waitForDispatch(store, "REQUEST_TABS_SUCCESS");
  // The test script will not close the toolbox and will timeout if it fails, so reaching
  // this point in the test is enough to assume the test was successful.
  info("Wait for the toolbox to close");
  await onToolboxClose;
  ok(true, "Addon toolbox closed");

  await removeTemporaryExtension(ADDON_NOBG_NAME, document);
  await removeTab(tab);
});

async function toolboxTestScript(toolbox, devtoolsTab) {
  const targetName = toolbox.target.name;
  const isAddonTarget = toolbox.target.isAddon;
  ok(isAddonTarget, "Toolbox target is an addon");
  is(targetName, ADDON_NOBG_NAME, "Toolbox has the expected target");

  const inspector = await toolbox.selectTool("inspector");

  let nodeActor;
  info(`Wait the fallback window to be fully loaded`);
  await asyncWaitUntil(async () => {
    nodeActor = await inspector.walker.querySelector(
      inspector.walker.rootNode,
      "h1"
    );
    return nodeActor && nodeActor.inlineTextChild;
  });

  info("Got a nodeActor with an inline text child");
  const actualValue = nodeActor.inlineTextChild._form.nodeValue;
  is(
    actualValue,
    "Your addon does not have any document opened yet.",
    "nodeActor has the expected inlineTextChild value"
  );

  info("Wait for all pending requests to settle on the DevToolsClient");
  await toolbox.target.client.waitForRequestsToSettle();

  await removeTab(devtoolsTab);
}
