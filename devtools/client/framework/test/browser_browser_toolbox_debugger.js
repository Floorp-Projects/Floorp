/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test asserts that the new debugger works from the browser toolbox process
// Its pass a big piece of Javascript string to the browser toolbox process via
// MOZ_TOOLBOX_TEST_SCRIPT env variable. It does that as test resources fetched from
// chrome://mochitests/ package isn't available from browser toolbox process.

// On debug test runner, it takes about 50s to run the test.
requestLongerTimeout(4);

const { fetch } = require("devtools/shared/DevToolsUtils");

const debuggerHeadURL = CHROME_URL_ROOT + "../../debugger/new/test/mochitest/head.js";
const testScriptURL = CHROME_URL_ROOT + "test_browser_toolbox_debugger.js";

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
  // First inject a very minimal head, with simplest assertion methods
  // and very common globals
  let testHead = (function () {
    const info = msg => dump(msg + "\n");
    const is = (a, b, description) => {
      let msg = "'" + JSON.stringify(a) + "' is equal to '" + JSON.stringify(b) + "'";
      if (description) {
        msg += " - " + description;
      }
      if (a !== b) {
        msg = "FAILURE: " + msg;
        dump(msg + "\n");
        throw new Error(msg);
      } else {
        msg = "SUCCESS: " + msg;
        dump(msg + "\n");
      }
    };
    const ok = (a, description) => {
      let msg = "'" + JSON.stringify(a) + "' is true";
      if (description) {
        msg += " - " + description;
      }
      if (!a) {
        msg = "FAILURE: " + msg;
        dump(msg + "\n");
        throw new Error(msg);
      } else {
        msg = "SUCCESS: " + msg;
        dump(msg + "\n");
      }
    };
    const registerCleanupFunction = () => {};

    const Cu = Components.utils;
    const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
    const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
    const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
  }).toSource().replace(/^\(function \(\) \{|\}\)$/g, "");
  // Stringify testHead's function and remove `(function {` prefix and `})` suffix
  // to ensure inner symbols gets exposed to next pieces of code

  // Then inject new debugger head file
  let { content } = yield fetch(debuggerHeadURL);
  let debuggerHead = content;
  // We remove its import of shared-head, which isn't available in browser toolbox process
  // And isn't needed thanks to testHead's symbols
  debuggerHead = debuggerHead.replace(/Services.scriptloader.loadSubScript[^\)]*\);/, "");

  // Finally, fetch the debugger test script that is going to be execute in the browser
  // toolbox process
  let testScript = (yield fetch(testScriptURL)).content;
  let source =
    "try {" + testHead + debuggerHead + testScript + "} catch (e) {" +
    "  dump('Exception: '+ e + ' at ' + e.fileName + ':' + " +
    "       e.lineNumber + '\\nStack: ' + e.stack + '\\n');" +
    "}";
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", source);
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
