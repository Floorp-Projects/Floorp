/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the basic features of the Browser Console.

"use strict";

const ObjectClient = require("devtools/shared/client/object-client");

const {
  gDevToolsBrowser,
} = require("devtools/client/framework/devtools-browser");

add_task(async function() {
  await addTab("about:blank");

  const hud = await BrowserConsoleManager.openBrowserConsoleOrFocus();

  const { message, actor } = await obtainObjectWithCPOW(hud);
  await testFrontEnd(hud, message);
  await testBackEnd(hud, actor);
});

async function obtainObjectWithCPOW(hud) {
  info("Add a message listener, it will receive an object with a CPOW");
  await hud.ui.evaluateJSAsync(`
    Services.ppmm.addMessageListener("cpow", function listener(message) {
      Services.ppmm.removeMessageListener("cpow", listener);
      console.log(message.objects);
      globalThis.result = message.objects;
    });
  `);

  info("Open the browser content toolbox and send a message");
  const toolbox = await gDevToolsBrowser.openContentProcessToolbox(gBrowser);
  await toolbox.target.activeConsole.evaluateJS(`
    let {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
    Services.cpmm.sendAsyncMessage("cpow", null, {cpow: {a:1}});
  `);

  info("Obtain the object with CPOW");
  const message = await waitFor(() => findMessage(hud, "cpow"));
  const result = await hud.ui.evaluateJSAsync("result");
  const { actor } = result.result;

  info("Cleanup");
  await hud.ui.evaluateJSAsync("delete globalThis.result;");
  const onToolboxDestroyed = toolbox.once("destroyed");
  toolbox.win.top.close();
  await onToolboxDestroyed;

  return { message, actor };
}

async function testFrontEnd(hud, message) {
  const oi = message.querySelector(".tree");
  const node = oi.querySelector(".tree-node");
  is(node.textContent, "Object { cpow: CPOW }", "Got an object with a CPOW");

  expandObjectInspectorNode(node);
  await waitFor(() => getObjectInspectorChildrenNodes(node).length > 0);

  const cpow = findObjectInspectorNodeChild(node, "cpow");
  const cpowObject = cpow.querySelector(".objectBox-object");
  is(cpowObject.textContent, "CPOW {  }", "Got the CPOW");

  expandObjectInspectorNode(cpow);
  is(getObjectInspectorChildrenNodes(cpow).length, 0, "CPOW has no children");
}

async function testBackEnd(hud, actor) {
  // Check that inspecting an object with CPOW doesn't throw in the server.
  // This would be done in a mochitest-chrome suite, but that doesn't run in
  // e10s, so it's harder to get ahold of a CPOW.
  info("Creating an ObjectClient with: " + actor);
  const objectClient = new ObjectClient(hud.ui.proxy.client, { actor });

  // Before the fix for Bug 1382833, this wouldn't resolve due to a CPOW error
  // in the ObjectActor.
  const prototypeAndProperties = await objectClient.getPrototypeAndProperties();

  // The CPOW is in the "cpow" property.
  const cpow = prototypeAndProperties.ownProperties.cpow.value;

  is(cpow.class, "CPOW", "The CPOW grip has the right class.");

  // Check that various protocol request methods work for the CPOW.
  const objClient = new ObjectClient(hud.ui.proxy.client, cpow);

  let response = await objClient.getPrototypeAndProperties();
  is(
    Reflect.ownKeys(response.ownProperties).length,
    0,
    "No property was retrieved."
  );
  is(response.ownSymbols.length, 0, "No symbol property was retrieved.");
  is(response.prototype.type, "null", "The prototype is null.");

  response = await objClient.enumProperties({ ignoreIndexedProperties: true });
  let slice = await response.iterator.slice(0, response.iterator.count);
  is(
    Reflect.ownKeys(slice.ownProperties).length,
    0,
    "No property was retrieved."
  );

  response = await objClient.enumProperties({});
  slice = await response.iterator.slice(0, response.iterator.count);
  is(
    Reflect.ownKeys(slice.ownProperties).length,
    0,
    "No property was retrieved."
  );

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

function findObjectInspectorNodeChild(node, nodeLabel) {
  return getObjectInspectorChildrenNodes(node).find(child => {
    const label = child.querySelector(".object-label");
    return label && label.textContent === nodeLabel;
  });
}
