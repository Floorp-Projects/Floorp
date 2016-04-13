/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// On debug test slave, it takes about 50s to run the test.
requestLongerTimeout(4);

add_task(function* runTest() {
  yield new Promise(done => {
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
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, "browser-toolbox-console-works");
      done();
    }, "browser-toolbox-console-works", false);
  });

  // Be careful, this JS function is going to be executed in the addon toolbox,
  // which lives in another process. So do not try to use any scope variable!
  let env = Components.classes["@mozilla.org/process/environment;1"].getService(Components.interfaces.nsIEnvironment);
  let testScript = function () {
    toolbox.selectTool("webconsole")
      .then(console => {
        let { jsterm } = console.hud;
        let js = "Services.obs.notifyObservers(null, 'browser-toolbox-console-works', null);";
        return jsterm.execute(js);
      })
      .then(() => toolbox.destroy());
  };
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new " + testScript);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
  });

  let { BrowserToolboxProcess } = Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});
  let closePromise;
  yield new Promise(onRun => {
    closePromise = new Promise(onClose => {
      info("Opening the browser toolbox\n");
      BrowserToolboxProcess.init(onClose, onRun);
    });
  });
  ok(true, "Browser toolbox started\n");

  yield onCustomMessage;
  ok(true, "Received the custom message");

  yield closePromise;
  ok(true, "Browser toolbox process just closed");
});
