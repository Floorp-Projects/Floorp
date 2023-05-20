/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check displaying object with __proto__ in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>test Object Inspector __proto__</h1>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  logAllStoreChanges(hud);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    const obj = Object.create(null);
    // eslint-disable-next-line no-proto
    obj.__proto__ = [];
    content.wrappedJSObject.console.log("oi-test", obj);
  });

  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-test"));
  const objectInspector = node.querySelector(".tree");
  ok(objectInspector, "Object is printed in the console");

  is(
    objectInspector.textContent.trim(),
    "Object { __proto__: [] }",
    "Object is displayed as expected"
  );

  objectInspector.querySelector(".arrow").click();
  await waitFor(() => node.querySelectorAll(".tree-node").length === 2);

  const __proto__Node = node.querySelector(".tree-node:last-of-type");
  ok(
    __proto__Node.textContent.includes("__proto__: Array []"),
    "__proto__ node is displayed as expected"
  );
});
