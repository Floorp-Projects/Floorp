/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the basic features of the Browser Console.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-console.html?" +
                 Date.now();
const TEST_FILE = "chrome://mochitests/content/browser/devtools/client/" +
                  "webconsole/test/mochitest/" +
                  "test-cu-reporterror.js";

const TEST_XHR_ERROR_URI = `http://example.com/404.html?${Date.now()}`;

const TEST_IMAGE = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/test-image.png";

const ObjectClient = require("devtools/shared/client/object-client");

add_task(async function() {
  await addTab(TEST_URI);

  const opened = waitForBrowserConsole();

  let hud = HUDService.getBrowserConsole();
  ok(!hud, "browser console is not open");
  info("wait for the browser console to open with ctrl-shift-j");
  EventUtils.synthesizeKey("j", { accelKey: true, shiftKey: true }, window);

  hud = await opened;
  ok(hud, "browser console opened");

  await setFilterState(hud, {
    netxhr: true
  });

  await testMessages(hud);
  await testCPOWInspection(hud);
  await resetFilters(hud);
});

async function testMessages(hud) {
  hud.ui.clearOutput(true);

  expectUncaughtException();

  executeSoon(() => {
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
  const error = Cu.evalInSandbox(`new Error("error from nuked globals");`, sandbox);
  Cu.reportError(error);
  Cu.nukeSandbox(sandbox);

  // Add a message from a content window.
  content.console.log("message from content window");

  // Test eval.
  hud.jsterm.execute("document.location.href");

  // Test eval frame script
  hud.jsterm.execute(
    `gBrowser.selectedBrowser.messageManager.loadFrameScript(` +
    `'data:application/javascript,console.log("framescript-message")', false);` +
    `"framescript-eval";`);

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
  await checkMessageExists(hud,
    "error thrown from test-cu-reporterror.js via Cu.reportError()");
  await checkMessageExists(hud, "error from nuked globals");
  await checkMessageExists(hud, "message from content window");
  await checkMessageExists(hud, "browser.xul");
  await checkMessageExists(hud, "framescript-eval");
  await checkMessageExists(hud, "framescript-message");
  await checkMessageExists(hud, "foobarException");
  await checkMessageExists(hud, "test-console.html");
  await checkMessageExists(hud, "404.html");
  await checkMessageExists(hud, "test-image.png");
}

async function testCPOWInspection(hud) {
  // Directly request evaluation to get an actor for the selected browser.
  // Note that this doesn't actually render a message, and instead allows us
  // us to assert that inspecting an object doesn't throw in the server.
  // This would be done in a mochitest-chrome suite, but that doesn't run in
  // e10s, so it's harder to get ahold of a CPOW.
  const cpowEval = await hud.jsterm.requestEvaluation("gBrowser.selectedBrowser");
  info("Creating an ObjectClient with: " + cpowEval.result.actor);

  const objectClient = new ObjectClient(hud.jsterm.hud.proxy.client, {
    actor: cpowEval.result.actor,
  });

  // Before the fix for Bug 1382833, this wouldn't resolve due to a CPOW error
  // in the ObjectActor.
  const prototypeAndProperties = await objectClient.getPrototypeAndProperties();

  // Just a sanity check to make sure a valid packet came back
  is(prototypeAndProperties.prototype.class, "XBL prototype JSClass",
    "Looks like a valid response");

  // The CPOW is in the _contentWindow property.
  const cpow = prototypeAndProperties.ownProperties._contentWindow.value;

  // But it's only a CPOW in e10s.
  const e10sCheck = await hud.jsterm.requestEvaluation(
    "Cu.isCrossProcessWrapper(gBrowser.selectedBrowser._contentWindow)");
  if (!e10sCheck.result) {
    is(cpow.class, "Window", "The object is not a CPOW.");
    return;
  }

  is(cpow.class, "CPOW: Window", "The CPOW grip has the right class.");

  // Check that various protocol request methods work for the CPOW.
  const objClient = new ObjectClient(hud.jsterm.hud.proxy.client, cpow);

  let response = await objClient.getPrototypeAndProperties();
  is(Reflect.ownKeys(response.ownProperties).length, 0, "No property was retrieved.");
  is(response.ownSymbols.length, 0, "No symbol property was retrieved.");
  is(response.prototype.type, "null", "The prototype is null.");

  response = await objClient.enumProperties({ignoreIndexedProperties: true});
  let slice = await response.iterator.slice(0, response.iterator.count);
  is(Reflect.ownKeys(slice.ownProperties).length, 0, "No property was retrieved.");

  response = await objClient.enumProperties({});
  slice = await response.iterator.slice(0, response.iterator.count);
  is(Reflect.ownKeys(slice.ownProperties).length, 0, "No property was retrieved.");

  response = await objClient.getOwnPropertyNames();
  is(response.ownPropertyNames.length, 0, "No property was retrieved.");

  response = await objClient.getProperty("x");
  is(response.descriptor, undefined, "The property does not exist.");

  response = await objClient.enumSymbols();
  slice = await response.iterator.slice(0, response.iterator.count);
  is(slice.ownSymbols.length, 0, "No symbol property was retrieved.");

  response = await objClient.getPrototype();
  is(response.prototype.type, "null", "The prototype is null.");

  response = await objClient.getDisplayString();
  is(response.displayString, "<cpow>", "The CPOW stringifies to <cpow>");
}

async function checkMessageExists(hud, msg) {
  info(`Checking "${msg}" was logged`);
  const message = await waitFor(() => findMessage(hud, msg));
  ok(message, `"${msg}" was logged`);
}
