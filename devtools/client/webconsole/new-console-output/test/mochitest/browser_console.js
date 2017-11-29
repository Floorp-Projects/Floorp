/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the basic features of the Browser Console, bug 587757.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html?" + Date.now();
const TEST_FILE = "chrome://mochitests/content/browser/devtools/client/" +
                  "webconsole/test/test-cu-reporterror.js";

const TEST_XHR_ERROR_URI = `http://example.com/404.html?${Date.now()}`;

const TEST_IMAGE = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/test-image.png";

const ObjectClient = require("devtools/shared/client/object-client");

add_task(function* () {
  yield loadTab(TEST_URI);

  let opened = waitForBrowserConsole();

  let hud = HUDService.getBrowserConsole();
  ok(!hud, "browser console is not open");
  info("wait for the browser console to open with ctrl-shift-j");
  EventUtils.synthesizeKey("j", { accelKey: true, shiftKey: true }, window);

  hud = yield opened;
  ok(hud, "browser console opened");
  yield testMessages(hud);
  yield testCPOWInspection(hud);
});

function testMessages(hud) {
  hud.jsterm.clearOutput(true);

  expectUncaughtException();
  executeSoon(() => {
    foobarExceptionBug587757();
  });

  // Add a message from a chrome window.
  hud.iframeWindow.console.log("bug587757a");

  // Check Cu.reportError stack.
  // Use another js script to not depend on the test file line numbers.
  Services.scriptloader.loadSubScript(TEST_FILE, hud.iframeWindow);

  // Bug 1348885: test that error from nuked globals do not throw
  let sandbox = new Cu.Sandbox(null, {
    wantComponents: false,
    wantGlobalProperties: ["URL", "URLSearchParams"],
  });
  let error = Cu.evalInSandbox(`
    new Error("1348885");
  `, sandbox);
  Cu.reportError(error);
  Cu.nukeSandbox(sandbox);

  // Add a message from a content window.
  content.console.log("bug587757b");

  // Test eval.
  hud.jsterm.execute("document.location.href");

  // Test eval frame script
  hud.jsterm.execute(`
    gBrowser.selectedBrowser.messageManager.loadFrameScript('data:application/javascript,console.log("framescript-message")', false);
    "framescript-eval";
  `);

  // Check for network requests.
  let xhr = new XMLHttpRequest();
  xhr.onload = () => console.log("xhr loaded, status is: " + xhr.status);
  xhr.open("get", TEST_URI, true);
  xhr.send();

  // Check for xhr error.
  let xhrErr = new XMLHttpRequest();
  xhrErr.onload = () => {
    console.log("xhr error loaded, status is: " + xhrErr.status);
  };
  xhrErr.open("get", TEST_XHR_ERROR_URI, true);
  xhrErr.send();

  // Check that Fetch requests are categorized as "XHR".
  fetch(TEST_IMAGE).then(() => { console.log("fetch loaded"); });

  return waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "chrome window console.log() is displayed",
        text: "bug587757a",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        name: "Cu.reportError is displayed",
        text: "bug1141222",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
        stacktrace: [{
          file: TEST_FILE,
          line: 2,
        }, {
          file: TEST_FILE,
          line: 4,
        },
        // Ignore the rest of the stack,
        // just assert Cu.reportError call site
        // and consoleOpened call
        ]
      },
      {
        name: "Error from nuked global works",
        text: "1348885",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
      {
        name: "content window console.log() is displayed",
        text: "bug587757b",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        name: "jsterm eval result",
        text: "browser.xul",
        category: CATEGORY_OUTPUT,
        severity: SEVERITY_LOG,
      },
      {
        name: "jsterm eval result 2",
        text: "framescript-eval",
        category: CATEGORY_OUTPUT,
        severity: SEVERITY_LOG,
      },
      {
        name: "frame script message",
        text: "framescript-message",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        name: "exception message",
        text: "foobarExceptionBug587757",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
      {
        name: "network message",
        text: "test-console.html",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_INFO,
        isXhr: true,
      },
      {
        name: "xhr error message",
        text: "404.html",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_ERROR,
        isXhr: true,
      },
      {
        name: "network message",
        text: "test-image.png",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_INFO,
        isXhr: true,
      },
    ],
  });
}

function* testCPOWInspection(hud) {
  // Directly request evaluation to get an actor for the selected browser.
  // Note that this doesn't actually render a message, and instead allows us
  // us to assert that inspecting an object doesn't throw in the server.
  // This would be done in a mochitest-chrome suite, but that doesn't run in
  // e10s, so it's harder to get ahold of a CPOW.
  let cpowEval = yield hud.jsterm.requestEvaluation("gBrowser.selectedBrowser");
  info("Creating an ObjectClient with: " + cpowEval.result.actor);

  let objectClient = new ObjectClient(hud.jsterm.hud.proxy.client, {
    actor: cpowEval.result.actor,
  });

  // Before the fix for Bug 1382833, this wouldn't resolve due to a CPOW error
  // in the ObjectActor.
  let prototypeAndProperties = yield objectClient.getPrototypeAndProperties();

  // Just a sanity check to make sure a valid packet came back
  is(prototypeAndProperties.prototype.class, "XBL prototype JSClass",
    "Looks like a valid response");

  // The CPOW is in the _contentWindow property.
  let cpow = prototypeAndProperties.ownProperties._contentWindow.value;

  // But it's only a CPOW in e10s.
  let e10sCheck = yield hud.jsterm.requestEvaluation(
    "Cu.isCrossProcessWrapper(gBrowser.selectedBrowser._contentWindow)");
  if (e10sCheck.result) {
    is(cpow.class, "CPOW: Window", "The CPOW grip has the right class.");
  } else {
    is(cpow.class, "Window", "The object is not a CPOW.");
  }
}
