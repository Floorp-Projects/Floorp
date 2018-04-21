/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = scopedCuImport("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

// On debug test slave, it takes about 50s to run the test.
requestLongerTimeout(4);

add_task(async function() {
  await new Promise(done => {
    let options = {"set": [
      ["devtools.debugger.prompt-connection", false],
      ["devtools.debugger.remote-enabled", true],
      ["devtools.chrome.enabled", true],
      // Test-only pref to allow passing `testScript` argument to the browser
      // toolbox
      ["devtools.browser-toolbox.allow-unsafe-script", true],
      // On debug test slave, it takes more than the default time (20s)
      // to get a initialized console
      ["devtools.debugger.remote-timeout", 120000]
    ]};
    SpecialPowers.pushPrefEnv(options, done);
  });

  // Wait for a notification sent by a script evaluated in the webconsole
  // of the browser toolbox.
  let onCustomMessage = new Promise(done => {
    Services.obs.addObserver(function listener(target, aTop, data) {
      Services.obs.removeObserver(listener, "browser-toolbox-console-works");
      done(data === "true");
    }, "browser-toolbox-console-works");
  });

  // Be careful, this JS function is going to be executed in the addon toolbox,
  // which lives in another process. So do not try to use any scope variable!
  let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  /* global toolbox */
  let testScript = function() {
    toolbox.selectTool("webconsole")
      .then(console => {
        // This is for checking Browser Toolbox doesn't have a close button.
        let hasCloseButton = !!toolbox.doc.getElementById("toolbox-close");
        let { jsterm } = console.hud;
        let js = "Services.obs.notifyObservers(null, 'browser-toolbox-console-works', " +
            hasCloseButton + ");";
        return jsterm.execute(js);
      })
      .then(() => toolbox.destroy());
  };
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new " + testScript);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
  });

  let { BrowserToolboxProcess } = ChromeUtils.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});
  is(BrowserToolboxProcess.getBrowserToolboxSessionState(), false, "No session state initially");

  let closePromise;
  await new Promise(onRun => {
    closePromise = new Promise(onClose => {
      info("Opening the browser toolbox\n");
      BrowserToolboxProcess.init(onClose, onRun);
    });
  });
  ok(true, "Browser toolbox started\n");
  is(BrowserToolboxProcess.getBrowserToolboxSessionState(), true, "Has session state");

  let hasCloseButton = await onCustomMessage;
  ok(true, "Received the custom message");
  ok(!hasCloseButton, "Browser toolbox doesn't have a close button");

  await closePromise;
  ok(true, "Browser toolbox process just closed");
  is(BrowserToolboxProcess.getBrowserToolboxSessionState(), false, "No session state after closing");
});
