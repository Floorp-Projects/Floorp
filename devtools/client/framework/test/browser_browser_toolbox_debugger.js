/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// On debug test runner, it takes about 50s to run the test.
requestLongerTimeout(4);

const { setInterval, clearInterval } = require("sdk/timers");

add_task(function* runTest() {
  yield new Promise(done => {
    let options = {"set": [
      ["devtools.debugger.prompt-connection", false],
      ["devtools.debugger.remote-enabled", true],
      ["devtools.chrome.enabled", true],
      // Test-only pref to allow passing `testScript` argument to the browser
      // toolbox
      ["devtools.browser-toolbox.allow-unsafe-script", true],
      // On debug test runner, it takes more than the default time (20s)
      // to get a initialized console
      ["devtools.debugger.remote-timeout", 120000]
    ]};
    SpecialPowers.pushPrefEnv(options, done);
  });

  let s = Cu.Sandbox("http://mozilla.org");
  // Pass a fake URL to evalInSandbox. If we just pass a filename,
  // Debugger is going to fail and only display root folder (`/`) listing.
  // But it won't try to fetch this url and use sandbox content as expected.
  let testUrl = "http://mozilla.org/browser-toolbox-test.js";
  Cu.evalInSandbox("(" + function () {
    this.plop = function plop() {
      return 1;
    };
  } + ").call(this)", s, "1.8", testUrl, 0);

  // Execute the function every second in order to trigger the breakpoint
  let interval = setInterval(s.plop, 1000);

  // Be careful, this JS function is going to be executed in the browser toolbox,
  // which lives in another process. So do not try to use any scope variable!
  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  let testScript = function () {
    const { Task } = Components.utils.import("resource://gre/modules/Task.jsm", {});
    dump("Opening the browser toolbox and debugger panel\n");
    let window, document;
    let testUrl = "http://mozilla.org/browser-toolbox-test.js";
    Task.spawn(function* () {
      dump("Waiting for debugger load\n");
      let panel = yield toolbox.selectTool("jsdebugger");
      let window = panel.panelWin;
      let document = window.document;

      yield window.once(window.EVENTS.SOURCE_SHOWN);

      dump("Loaded, selecting the test script to debug\n");
      let item = document.querySelector(`.dbg-source-item[tooltiptext="${testUrl}"]`);
      let onSourceShown = window.once(window.EVENTS.SOURCE_SHOWN);
      item.click();
      yield onSourceShown;

      dump("Selected, setting a breakpoint\n");
      let { Sources, editor } = window.DebuggerView;
      let onBreak = window.once(window.EVENTS.FETCHED_SCOPES);
      editor.emit("gutterClick", 1);
      yield onBreak;

      dump("Paused, asserting breakpoint position\n");
      let url = Sources.selectedItem.attachment.source.url;
      if (url != testUrl) {
        throw new Error("Breaking on unexpected script: " + url);
      }
      let cursor = editor.getCursor();
      if (cursor.line != 1) {
        throw new Error("Breaking on unexpected line: " + cursor.line);
      }

      dump("Now, stepping over\n");
      let stepOver = window.document.querySelector("#step-over");
      let onFetchedScopes = window.once(window.EVENTS.FETCHED_SCOPES);
      stepOver.click();
      yield onFetchedScopes;

      dump("Stepped, asserting step position\n");
      url = Sources.selectedItem.attachment.source.url;
      if (url != testUrl) {
        throw new Error("Stepping on unexpected script: " + url);
      }
      cursor = editor.getCursor();
      if (cursor.line != 2) {
        throw new Error("Stepping on unexpected line: " + cursor.line);
      }

      dump("Resume script execution\n");
      let resume = window.document.querySelector("#resume");
      let onResume = toolbox.target.once("thread-resumed");
      resume.click();
      yield onResume;

      dump("Close the browser toolbox\n");
      toolbox.destroy();

    }).catch(error => {
      dump("Error while running code in the browser toolbox process:\n");
      dump(error + "\n");
      dump("stack:\n" + error.stack + "\n");
    });
  };
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new " + testScript);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
  });

  let { BrowserToolboxProcess } = Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});
  // Use two promises, one for each BrowserToolboxProcess.init callback
  // arguments, to ensure that we wait for toolbox run and close events.
  let closePromise;
  yield new Promise(onRun => {
    closePromise = new Promise(onClose => {
      info("Opening the browser toolbox\n");
      BrowserToolboxProcess.init(onClose, onRun);
    });
  });
  ok(true, "Browser toolbox started\n");

  yield closePromise;
  ok(true, "Browser toolbox process just closed");

  clearInterval(interval);
});
