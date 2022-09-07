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
      background() {
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

  info("Check that the color scheme simulation buttons are hidden");
  const lightButtonIsHidden = inspector.panelDoc
    .querySelector("#color-scheme-simulation-light-toggle")
    ?.hasAttribute("hidden");
  const darkButtonIsHidded = inspector.panelDoc
    .querySelector("#color-scheme-simulation-dark-toggle")
    ?.hasAttribute("hidden");
  ok(
    lightButtonIsHidden,
    "The light color scheme simulation button exists and is hidden"
  );
  ok(
    darkButtonIsHidded,
    "The dark color scheme simulation button exists and is hidden"
  );

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});
