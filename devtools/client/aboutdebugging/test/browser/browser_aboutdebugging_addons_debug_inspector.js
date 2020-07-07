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

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - the webextension developer toolbox has a working Inspector panel, with the
 *   background page as default target;
 */
add_task(async function testWebExtensionsToolboxWebConsole() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background: function() {
        document.body.innerText = "Background Page Body Test Content";
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  info("Open a toolbox to debug the addon");
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);

  const onToolboxClose = gDevTools.once("toolbox-destroyed");
  toolboxTestScript(toolbox, devtoolsTab);

  // The test script will not close the toolbox and will timeout if it fails, so reaching
  // this point in the test is enough to assume the test was successful.
  info("Wait for the toolbox to close");
  await onToolboxClose;
  ok(true, "Addon toolbox closed");

  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});

async function toolboxTestScript(toolbox, devtoolsTab) {
  const inspector = await toolbox.selectTool("inspector");
  const nodeActor = await inspector.walker.querySelector(
    inspector.walker.rootNode,
    "body"
  );
  ok(nodeActor, "Got a nodeActor");
  ok(nodeActor.inlineTextChild, "Got a nodeActor with an inline text child");

  const actualValue = nodeActor.inlineTextChild._form.nodeValue;

  is(
    String(actualValue).trim(),
    "Background Page Body Test Content",
    "nodeActor has the expected inlineTextChild value"
  );

  info("Wait for all pending requests to settle on the DevToolsClient");
  await toolbox.target.client.waitForRequestsToSettle();

  await removeTab(devtoolsTab);
}
