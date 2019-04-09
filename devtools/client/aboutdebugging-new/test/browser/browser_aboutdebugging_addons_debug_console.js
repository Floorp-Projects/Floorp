/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

// This is a migration from:
// https://searchfox.org/mozilla-central/source/devtools/client/aboutdebugging/test/browser_addons_debug_webextension.js

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - when the debug button is clicked on a webextension, the opened toolbox
 *   has a working webconsole with the background page as default target;
 */
add_task(async function testWebExtensionsToolboxWebConsole() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI({
    background: function() {
      window.myWebExtensionAddonFunction = function() {
        console.log("Background page function called",
                    this.browser.runtime.getManifest());
      };
    },
    id: ADDON_ID,
    name: ADDON_NAME,
  }, document);

  const { devtoolsTab, devtoolsWindow } =
    await openAboutDevtoolsToolbox(document, tab, window, ADDON_NAME);
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

function toolboxTestScript(toolbox, devtoolsTab) {
  function findMessages(hud, text, selector = ".message") {
    const messages = hud.ui.outputNode.querySelectorAll(selector);
    const elements = Array.prototype.filter.call(
      messages,
      (el) => el.textContent.includes(text)
    );
    return elements;
  }

  async function waitFor(condition) {
    while (!condition()) {
      await new Promise(done => toolbox.win.setTimeout(done, 1000));
    }
  }

  toolbox.selectTool("webconsole")
    .then(async console => {
      const { hud } = console;
      const { jsterm } = hud;
      const onMessage = waitFor(() => {
        return findMessages(hud, "Background page function called").length > 0;
      });
      await jsterm.execute("myWebExtensionAddonFunction()");
      await onMessage;
      await removeTab(devtoolsTab);
    })
    .catch(e => dump("Exception from browser toolbox process: " + e + "\n"));
}
