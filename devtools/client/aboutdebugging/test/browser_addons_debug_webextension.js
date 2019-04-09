/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - when the debug button is clicked on a webextension, the opened toolbox
 *   has a working webconsole with the background page as default target;
 */
add_task(async function testWebExtensionsToolboxWebConsole() {
  const addonFile = ExtensionTestCommon.generateXPI({
    background: function() {
      window.myWebExtensionAddonFunction = function() {
        console.log("Background page function called",
                    this.browser.runtime.getManifest());
      };
    },
    manifest: {
      name: ADDON_NAME,
      applications: {
        gecko: {id: ADDON_ID},
      },
    },
  });
  registerCleanupFunction(() => addonFile.remove(false));

  const {
    tab, document, debugBtn,
  } = await setupTestAboutDebuggingWebExtension(ADDON_NAME, addonFile);

  const onToolboxReady = gDevTools.once("toolbox-ready");
  const onToolboxClose = gDevTools.once("toolbox-destroyed");
  debugBtn.click();
  const toolbox = await onToolboxReady;
  testScript(toolbox);

  await onToolboxClose;
  ok(true, "Addon toolbox closed");

  await uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  await closeAboutDebugging(tab);
});

const testScript = function(toolbox) {
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
      await new Promise(done => window.setTimeout(done, 1000));
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
           await toolbox.destroy();
         })
         .catch(e => dump("Exception from browser toolbox process: " + e + "\n"));
};
