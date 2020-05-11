/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the basic features of the Browser Console.

"use strict";

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
  // Needed for the execute() function below
  await pushPref("security.allow_parent_unrestricted_js_loads", true);
  await pushPref("devtools.browserconsole.contentMessages", true);
  // Bug 1605036: Disable Multiprocess Browser Toolbox for now as it introduces intermittent failure in this test
  await pushPref("devtools.browsertoolbox.fission", false);

  await addTab(TEST_URI);

  const opened = waitForBrowserConsole();

  let hud = BrowserConsoleManager.getBrowserConsole();
  ok(!hud, "browser console is not open");
  info("wait for the browser console to open with ctrl-shift-j");
  EventUtils.synthesizeKey("j", { accelKey: true, shiftKey: true }, window);

  hud = await opened;
  ok(hud, "browser console opened");

  await setFilterState(hud, {
    netxhr: true,
  });

  await testMessages(hud);
  await resetFilters(hud);
});

async function testMessages(hud) {
  await clearOutput(hud);

  executeSoon(() => {
    expectUncaughtException();
    // eslint-disable-next-line no-undef
    foobarException();
  });

  // Add a message from a chrome window.
  hud.iframeWindow.console.log("message from chrome window");

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

  // Add a message from a content window.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.console.log("message from content window");
  });

  // Test eval.
  execute(hud, "document.location.href");

  // Test eval frame script
  execute(
    hud,
    `gBrowser.selectedBrowser.messageManager.loadFrameScript(` +
      `'data:application/javascript,console.log("framescript-message")', false);` +
      `"framescript-eval";`
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

  await checkMessageExists(hud, "message from chrome window");
  await checkMessageExists(
    hud,
    "error thrown from test-cu-reporterror.js via Cu.reportError()"
  );
  await checkMessageExists(hud, "error from nuked globals");
  await checkMessageExists(hud, "message from content window");
  await checkMessageExists(hud, "browser.xhtml");
  await checkMessageExists(hud, "framescript-eval");
  await checkMessageExists(hud, "framescript-message");
  await checkMessageExists(hud, "foobarException");
  await checkMessageExists(hud, "test-console.html");
  await checkMessageExists(hud, "404.html");
  await checkMessageExists(hud, "test-image.png");
}

async function checkMessageExists(hud, msg) {
  info(`Checking "${msg}" was logged`);
  const message = await waitFor(
    () => findMessage(hud, msg),
    `Couldn't find "${msg}"`
  );
  ok(message, `"${msg}" was logged`);
}
