/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the basic features of the Browser Console.

"use strict";

const {
  gDevToolsBrowser,
} = require("devtools/client/framework/devtools-browser");

add_task(async function() {
  // Needed for the execute() function below
  await pushPref("security.allow_parent_unrestricted_js_loads", true);

  await addTab("about:blank");

  const hud = await BrowserConsoleManager.openBrowserConsoleOrFocus();

  const { message, objectFront } = await obtainObjectWithCPOW(hud);
  await testFrontEnd(message);
  await testBackEnd(objectFront);
  await clearOutput(hud, true);
});

async function obtainObjectWithCPOW(hud) {
  info("Add a message listener, it will receive an object with a CPOW");
  await hud.evaluateJSAsync(`
    Services.ppmm.addMessageListener("cpow", function listener(message) {
      Services.ppmm.removeMessageListener("cpow", listener);
      console.log(message.objects);
      globalThis.result = message.objects;
    });
  `);

  info("Open the browser content toolbox and send a message");
  const toolbox = await gDevToolsBrowser.openContentProcessToolbox(gBrowser);
  const webConsoleFront = await toolbox.target.getFront("console");
  await webConsoleFront.evaluateJSAsync(`
    let {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
    Services.cpmm.sendAsyncMessage("cpow", null, {cpow: {a:1}});
  `);

  info("Obtain the object with CPOW");
  const message = await waitFor(() => findMessage(hud, "cpow"));
  const result = await hud.evaluateJSAsync("result");
  const objectFront = result.result;

  info("Cleanup");
  await hud.evaluateJSAsync("delete globalThis.result;");
  const onToolboxDestroyed = toolbox.once("destroyed");
  toolbox.topWindow.close();
  await onToolboxDestroyed;

  return { message, objectFront };
}

async function testFrontEnd(message) {
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

async function testBackEnd(objectFront) {
  // Check that inspecting an object with CPOW doesn't throw in the server.
  // This would be done in a mochitest-chrome suite, but that doesn't run in
  // e10s, so it's harder to get ahold of a CPOW.

  // Before the fix for Bug 1382833, this wouldn't resolve due to a CPOW error
  // in the ObjectActor.
  const prototypeAndProperties = await objectFront.getPrototypeAndProperties();

  // The CPOW is in the "cpow" property.
  const cpowFront = prototypeAndProperties.ownProperties.cpow.value;

  is(cpowFront.getGrip().class, "CPOW", "The CPOW grip has the right class.");

  // Check that various protocol request methods work for the CPOW.
  let response = await cpowFront.getPrototypeAndProperties();
  is(
    Reflect.ownKeys(response.ownProperties).length,
    0,
    "No property was retrieved."
  );
  is(response.ownSymbols.length, 0, "No symbol property was retrieved.");
  is(response.prototype.type, "null", "The prototype is null.");

  response = await cpowFront.enumProperties({ ignoreIndexedProperties: true });
  let slice = await response.slice(0, response.count);
  is(
    Reflect.ownKeys(slice.ownProperties).length,
    0,
    "No property was retrieved."
  );

  response = await cpowFront.enumProperties({});
  slice = await response.slice(0, response.count);
  is(
    Reflect.ownKeys(slice.ownProperties).length,
    0,
    "No property was retrieved."
  );

  response = await cpowFront.getOwnPropertyNames();
  is(response.ownPropertyNames.length, 0, "No property was retrieved.");

  response = await cpowFront.getProperty("x");
  is(response.descriptor, undefined, "The property does not exist.");

  response = await cpowFront.enumSymbols();
  slice = await response.slice(0, response.count);
  is(slice.ownSymbols.length, 0, "No symbol property was retrieved.");

  response = await cpowFront.getPrototype();
  is(response.prototype.type, "null", "The prototype is null.");

  response = await cpowFront.getDisplayString();
  is(response.displayString, "<cpow>", "The CPOW stringifies to <cpow>");
}

function findObjectInspectorNodeChild(node, nodeLabel) {
  return getObjectInspectorChildrenNodes(node).find(child => {
    const label = child.querySelector(".object-label");
    return label && label.textContent === nodeLabel;
  });
}
