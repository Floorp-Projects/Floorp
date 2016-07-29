/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
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

  // Wait for a notification sent by a script evaluated the test addon via
  // the web console.
  let onCustomMessage = new Promise(done => {
    Services.obs.addObserver(function listener(message, topic) {
      let apiMessage = message.wrappedJSObject;
      if (!apiMessage.originAttributes ||
          apiMessage.originAttributes.addonId != ADDON_ID) {
        return;
      }
      Services.obs.removeObserver(listener, "console-api-log-event");
      done(apiMessage.arguments);
    }, "console-api-log-event", false);
  });

  // Be careful, this JS function is going to be executed in the addon toolbox,
  // which lives in another process. So do not try to use any scope variable!
  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
  let testScript = function () {
    /* eslint-disable no-undef */
    toolbox.selectTool("webconsole")
      .then(console => {
        let { jsterm } = console.hud;
        return jsterm.execute("myWebExtensionAddonFunction()");
      })
      .then(() => toolbox.destroy());
    /* eslint-enable no-undef */
  };
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new " + testScript);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
  });

  let onToolboxClose = BrowserToolboxProcess.once("close");

  debugBtn.click();

  let args = yield onCustomMessage;
  ok(true, "Received console message from the background page function as expected");
  is(args[0], "Background page function called", "Got the expected console message");
  is(args[1] && args[1].name, ADDON_NAME,
     "Got the expected manifest from WebExtension API");

  yield onToolboxClose;
  ok(true, "Addon toolbox closed");

  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  yield closeAboutDebugging(tab);
});
