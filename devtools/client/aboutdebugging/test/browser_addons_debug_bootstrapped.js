/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

const { BrowserToolboxProcess } = Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});

add_task(function* () {
  yield new Promise(resolve => {
    let options = {"set": [
      // Force enabling of addons debugging
      ["devtools.chrome.enabled", true],
      ["devtools.debugger.remote-enabled", true],
      // Disable security prompt
      ["devtools.debugger.prompt-connection", false],
      // Enable Browser toolbox test script execution via env variable
      ["devtools.browser-toolbox.allow-unsafe-script", true],
    ]};
    SpecialPowers.pushPrefEnv(options, resolve);
  });

  let { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);
  yield installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: ADDON_NAME,
  });

  // Retrieve the DEBUG button for the addon
  let names = [...document.querySelectorAll("#addons .target-name")];
  let name = names.filter(element => element.textContent === ADDON_NAME)[0];
  ok(name, "Found the addon in the list");
  let targetElement = name.parentNode.parentNode;
  let debugBtn = targetElement.querySelector(".debug-button");
  ok(debugBtn, "Found its debug button");

  // Wait for a notification sent by a script evaluated the test addon via
  // the web console.
  let onCustomMessage = new Promise(done => {
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, "addon-console-works");
      done();
    }, "addon-console-works", false);
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
        return jsterm.execute("myBootstrapAddonFunction()");
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

  yield onCustomMessage;
  ok(true, "Received the notification message from the bootstrap.js function");

  yield onToolboxClose;
  ok(true, "Addon toolbox closed");

  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  yield closeAboutDebugging(tab);
});
