/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the basic features of the Browser Console.

"use strict";

requestLongerTimeout(2);

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html?" +
  Date.now();

const TEST_XHR_ERROR_URI = `http://example.com/404.html?${Date.now()}`;

const TEST_IMAGE =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/test-image.png";

add_task(async function () {
  // Needed for the execute() call in `testMessages`.
  await pushPref("security.allow_parent_unrestricted_js_loads", true);
  await pushPref("devtools.browserconsole.enableNetworkMonitoring", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");

  // Open a parent process tab to check it doesn't have impact
  const aboutRobotsTab = await addTab("about:robots");
  // And open the "actual" test tab
  const tab = await addTab(TEST_URI);

  await testMessages();

  info("Close tab");
  await removeTab(tab);
  await removeTab(aboutRobotsTab);
});

async function testMessages() {
  const opened = waitForBrowserConsole();
  let hud = BrowserConsoleManager.getBrowserConsole();
  ok(!hud, "browser console is not open");

  // The test harness does override the global's console property to replace it with
  // a Console.sys.mjs instance (https://searchfox.org/mozilla-central/rev/c5c002f81f08a73e04868e0c2bf0eb113f200b03/testing/mochitest/api.js#75-78)
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

  // Spawn Chrome worker from a chrome window and log a message
  // It's important to use the browser console global so the message gets assigned
  // a non-numeric innerID in Console.cpp
  const browserConsoleGlobal = Cu.getGlobalForObject(hud);
  const chromeWorker = new browserConsoleGlobal.ChromeWorker(
    URL.createObjectURL(
      new browserConsoleGlobal.Blob(
        [`console.log("message in chrome worker")`],
        {
          type: "application/javascript",
        }
      )
    )
  );

  const sandbox = new Cu.Sandbox(null, {
    wantComponents: false,
    wantGlobalProperties: ["URL", "URLSearchParams"],
  });
  const error = Cu.evalInSandbox(
    `new Error("error from nuked globals");`,
    sandbox
  );
  console.error(error);
  Cu.nukeSandbox(sandbox);

  const componentsException = new Components.Exception("Components.Exception");
  console.error(componentsException);

  // Check privileged error message from a content process
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    (async function () {
      throw new Error("privileged content process error message");
    })();
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

  // Check messages logged in content with Log.sys.mjs
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const { Log } = ChromeUtils.importESModule(
      "resource://gre/modules/Log.sys.mjs"
    );
    const logger = Log.repository.getLogger("TEST_LOGGER_" + Date.now());
    logger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
    logger.level = Log.Level.Info;
    logger.info("Log.sys.mjs content process messsage");
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
    "Log.sys.mjs content process messsage",
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
    "message in chrome worker",
    ".console-api"
  );
  await checkUniqueMessageExists(
    hud,
    "Expected color but found ‘rainbow’",
    ".warn"
  );
  await checkUniqueMessageExists(
    hud,
    "Expected color but found ‘bled’",
    ".warn"
  );

  await checkComponentExceptionMessage(hud, componentsException);

  await resetFilters(hud);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.testWorker.terminate();
    delete content.testWorker;
  });
  chromeSpawnedWorker.terminate();
  chromeWorker.terminate();
  info("Close the Browser Console");
  await safeCloseBrowserConsole();
}

async function checkComponentExceptionMessage(hud, exception) {
  const msgNode = await checkUniqueMessageExists(
    hud,
    "Components.Exception",
    ".error"
  );
  const framesNode = await waitFor(() => msgNode.querySelector(".pane.frames"));
  ok(framesNode, "The Components.Exception stack is displayed right away");

  const frameNodes = framesNode.querySelectorAll(".frame");
  ok(frameNodes.length > 1, "Got at least one frame in the stack");
  is(
    frameNodes[0].querySelector(".line").textContent,
    String(exception.lineNumber),
    "The stack displayed by default refers to Components.Exception passed as argument"
  );

  const [, line] = msgNode
    .querySelector(".frame-link-line")
    .textContent.split(":");
  is(
    line,
    String(exception.lineNumber + 1),
    "The link on the top right refers to the console.error callsite"
  );
}
