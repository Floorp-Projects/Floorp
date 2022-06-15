/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check evaluating and expanding getters on an array in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>Object Inspector on Getters on an Array</h1>";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const array = [];
    Object.defineProperty(array, 0, {
      get: () => "elem0",
    });
    Object.defineProperty(array, 1, {
      set: x => {},
    });
    Object.defineProperty(array, 2, {
      get: () => "elem2",
      set: x => {},
    });
    content.wrappedJSObject.console.log("oi-array-test", array);
  });

  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-array-test"));
  const oi = node.querySelector(".tree");

  const arrayText = oi.querySelector(".objectBox-array");

  is(
    arrayText.textContent,
    "Array(3) [ Getter, Setter, Getter & Setter ]",
    "Elements with getter/setter should be shown correctly"
  );

  expandObjectInspectorNode(oi);
  await waitFor(() => getObjectInspectorNodes(oi).length > 1);

  await testGetter(oi, "0");
  await testSetterOnly(oi, "1");
  await testGetter(oi, "2");
});

async function testGetter(oi, index) {
  let node = findObjectInspectorNode(oi, index);
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    `The ${index} node can't be expanded`
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, `There is an invoke button for ${index} as expected`);

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(findObjectInspectorNode(oi, index))
  );

  node = findObjectInspectorNode(oi, index);
  ok(
    node.textContent.includes(`${index}: "elem${index}"`),
    "Element ${index} now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    `The ${index} node can't be expanded`
  );
}

async function testSetterOnly(oi, index) {
  const node = findObjectInspectorNode(oi, index);
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    `The ${index} node can't be expanded`
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(!invokeButton, `There is no invoke button for ${index}`);

  ok(
    node.textContent.includes(`${index}: Setter`),
    "Element ${index} now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    `The ${index} node can't be expanded`
  );
}
