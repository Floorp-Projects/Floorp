/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
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
  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NOBG_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);

  ok(
    toolbox.commands.descriptorFront.isWebExtensionDescriptor,
    "Toolbox is debugging an addon"
  );
  const targetName = toolbox.target.name;
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

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await removeTemporaryExtension(ADDON_NOBG_NAME, document);
  await removeTab(tab);
});
