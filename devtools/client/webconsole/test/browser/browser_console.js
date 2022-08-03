/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the basic features of the Browser Console.

"use strict";

requestLongerTimeout(2);

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html?" +
  Date.now();
const TEST_FILE =
  "chrome://mochitests/content/browser/devtools/client/" +
  "webconsole/test/browser/" +
  "test-cu-reporterror.js";

const TEST_XHR_ERROR_URI = `http://example.com/404.html?${Date.now()}`;

const TEST_IMAGE =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/test-image.png";

add_task(async function() {
  // Needed for the execute() call in `testMessages`.
  await pushPref("security.allow_parent_unrestricted_js_loads", true);
  await pushPref("devtools.browserconsole.contentMessages", true);
  await pushPref("devtools.browserconsole.enableNetworkMonitoring", true);
  const tab = await addTab(TEST_URI);

  info(
    "Check browser console messages with devtools.browsertoolbox.fission set to false"
  );
  await pushPref("devtools.browsertoolbox.fission", false);
  await testMessages();

  info(
    "Check browser console messages with devtools.browsertoolbox.fission set to true"
  );
  await pushPref("devtools.browsertoolbox.scope", "everything");
  await pushPref("devtools.browsertoolbox.fission", true);
  await testMessages();

  info("Close tab");
  await removeTab(tab);
});

async function testMessages() {
  const opened = waitForBrowserConsole();
  let hud = BrowserConsoleManager.getBrowserConsole();
  ok(!hud, "browser console is not open");

  // The test harness does override the global's console property to replace it with
  // a Console.jsm instance (https://searchfox.org/mozilla-central/rev/618f9970972adc5a21194d39d690ec0865f26024/testing/mochitest/api.js#75-80)
  // So here we reset the console property with the native console (which is luckily
  // stored in `nativeConsole`).
  const overriddenConsole = globalThis.console;
  globalThis.console = globalThis.nativeConsole;

  info("wait for the browser console to open with ctrl-shift-j");
  EventUtils.synthesizeKey("j", { accelKey: true, shiftKey: true }, window);

  hud = await opened;
  ok(hud, "browser console opened");

  info("Check that we don't display the non-native console API warning");
  // Wait a bit to let room for the message to be displayed
  await wait(1000);
  is(
    await findMessageVirtualizedByType({
      hud,
      text: "The Web Console logging API",
      typeSelector: ".warn",
    }),
    undefined,
    "The message about disabled console API is not displayed"
  );
  // Set the overidden console back.
  globalThis.console = overriddenConsole;

  await clearOutput(hud);

  await setFilterState(hud, {
    netxhr: true,
    css: true,
  });

  executeSoon(() => {
    expectUncaughtException();
    // eslint-disable-next-line no-undef
    foobarException();
  });

  // Add a message from a chrome window.
  hud.iframeWindow.console.log("message from chrome window");

  // Spawn worker from a chrome window and log a message and an error
  const workerCode = `console.log("message in parent worker");
        throw new Error("error in parent worker");`;
  const blob = new hud.iframeWindow.Blob([workerCode], {
    type: "application/javascript",
  });
  const chromeSpawnedWorker = new hud.iframeWindow.Worker(
    URL.createObjectURL(blob)
  );

  // Check Cu.reportError stack.
  // Use another js script to not depend on the test file line numbers.
  Services.scriptloader.loadSubScript(TEST_FILE, hud.iframeWindow);

  const sandbox = new Cu.Sandbox(null, {
    wantComponents: false,
    wantGlobalProperties: ["URL", "URLSearchParams"],
  });
  const error = Cu.evalInSandbox(
    `new Error("error from nuked globals");`,
    sandbox
  );
  Cu.reportError(error);
  Cu.nukeSandbox(sandbox);

  // Check privileged error message from a content process
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    Cu.reportError("privileged content process error message");
  });

  // Add a message from a content window.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.console.log("message from content window");
    content.wrappedJSObject.throwError("error from content window");

    content.testWorker = new content.Worker("./test-worker.js");
    content.testWorker.postMessage({
      type: "log",
      message: "message in content worker",
    });
    content.testWorker.postMessage({
      type: "error",
      message: "error in content worker",
    });
  });

  // Test eval.
  execute(hud, "`Parent Process Location: ${document.location.href}`");

  // Test eval frame script
  gBrowser.selectedBrowser.messageManager.loadFrameScript(
    `data:application/javascript,console.log("framescript-message")`,
    false
  );

  // Check for network requests.
  const xhr = new XMLHttpRequest();
  xhr.onload = () => console.log("xhr loaded, status is: " + xhr.status);
  xhr.open("get", TEST_URI, true);
  xhr.send();

  // Check for xhr error.
  const xhrErr = new XMLHttpRequest();
  xhrErr.onload = () => {
    console.log("xhr error loaded, status is: " + xhrErr.status);
  };
  xhrErr.open("get", TEST_XHR_ERROR_URI, true);
  xhrErr.send();

  // Check that Fetch requests are categorized as "XHR".
  await fetch(TEST_IMAGE);
  console.log("fetch loaded");

  // Check messages logged with Services.console.logMessage
  const scriptErrorMessage = Cc["@mozilla.org/scripterror;1"].createInstance(
    Ci.nsIScriptError
  );
  scriptErrorMessage.initWithWindowID(
    "Error from Services.console.logMessage",
    gBrowser.currentURI.prePath,
    null,
    0,
    0,
    Ci.nsIScriptError.warningFlag,
    // platform-specific category to test case for Bug 1770160
    "chrome javascript",
    gBrowser.selectedBrowser.innerWindowID
  );
  Services.console.logMessage(scriptErrorMessage);

  // Check messages logged in content with Log.jsm
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
    const logger = Log.repository.getLogger("TEST_LOGGER_" + Date.now());
    logger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
    logger.level = Log.Level.Info;
    logger.info("Log.jsm content process messsage");
  });

  // Check CSS warnings in parent process
  await execute(hud, `document.body.style.backgroundColor = "rainbow"`);

  // Wait enough so any duplicated message would have the time to be rendered
  await wait(1000);

  await checkUniqueMessageExists(
    hud,
    "message from chrome window",
    ".console-api"
  );
  await checkUniqueMessageExists(
    hud,
    "error thrown from test-cu-reporterror.js via Cu.reportError()",
    ".error"
  );
  await checkUniqueMessageExists(hud, "error from nuked globals", ".error");
  await checkUniqueMessageExists(
    hud,
    "privileged content process error message",
    ".error"
  );
  await checkUniqueMessageExists(
    hud,
    "message from content window",
    ".console-api"
  );
  await checkUniqueMessageExists(hud, "error from content window", ".error");
  await checkUniqueMessageExists(
    hud,
    `"Parent Process Location: chrome://browser/content/browser.xhtml"`,
    ".result"
  );
  await checkUniqueMessageExists(hud, "framescript-message", ".console-api");
  await checkUniqueMessageExists(
    hud,
    "Error from Services.console.logMessage",
    ".warn"
  );
  await checkUniqueMessageExists(hud, "foobarException", ".error");
  await checkUniqueMessageExists(hud, "test-console.html", ".network");
  await checkUniqueMessageExists(hud, "404.html", ".network");
  await checkUniqueMessageExists(hud, "test-image.png", ".network");
  await checkUniqueMessageExists(
    hud,
    "Log.jsm content process messsage",
    ".console-api"
  );
  await checkUniqueMessageExists(
    hud,
    "message in content worker",
    ".console-api"
  );
  await checkUniqueMessageExists(hud, "error in content worker", ".error");
  await checkUniqueMessageExists(
    hud,
    "message in parent worker",
    ".console-api"
  );
  await checkUniqueMessageExists(hud, "error in parent worker", ".error");
  await checkUniqueMessageExists(
    hud,
    "Expected color but found ‘rainbow’",
    ".warn"
  );
  // CSS messages are only available in Browser Console when the pref is enabled
  if (SpecialPowers.getBoolPref("devtools.browsertoolbox.fission", false)) {
    await checkUniqueMessageExists(
      hud,
      "Expected color but found ‘bled’",
      ".warn"
    );
  }

  await resetFilters(hud);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.testWorker.terminate();
    delete content.testWorker;
  });
  chromeSpawnedWorker.terminate();
  info("Close the Browser Console");
  await safeCloseBrowserConsole();
}
