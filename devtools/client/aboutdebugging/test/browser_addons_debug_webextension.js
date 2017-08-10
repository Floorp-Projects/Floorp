/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";
const ADDON_MANIFEST_PATH = "addons/test-devtools-webextension/manifest.json";

const {
  BrowserToolboxProcess
} = Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - when the debug button is clicked on a webextension, the opened toolbox
 *   has a working webconsole with the background page as default target;
 */
add_task(function* testWebExtensionsToolboxWebConsole() {
  let {
    tab, document, debugBtn,
  } = yield setupTestAboutDebuggingWebExtension(ADDON_NAME, ADDON_MANIFEST_PATH);

  // Be careful, this JS function is going to be executed in the addon toolbox,
  // which lives in another process. So do not try to use any scope variable!
  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
  let testScript = function () {
    /* eslint-disable no-undef */
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
        let { hud } = console;
        let { jsterm } = hud;
        let onMessage = waitFor(() => {
          return findMessages(hud, "Background page function called").length > 0;
        });
        await jsterm.execute("myWebExtensionAddonFunction()");
        await onMessage;
        await toolbox.destroy();
      })
      .catch(e => dump("Exception from browser toolbox process: " + e + "\n"));
    /* eslint-enable no-undef */
  };
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new " + testScript);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
  });

  let onToolboxClose = BrowserToolboxProcess.once("close");

  debugBtn.click();

  yield onToolboxClose;
  ok(true, "Addon toolbox closed");

  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  yield closeAboutDebugging(tab);
});
