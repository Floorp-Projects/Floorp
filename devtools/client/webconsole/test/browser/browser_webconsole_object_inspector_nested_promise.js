/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check evaluating and expanding promises in the console.
const TEST_URI =
  "data:text/html;charset=utf8," +
  "<h1>Object Inspector on deeply nested promises</h1>";

add_task(async function testExpandNestedPromise() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    let nestedPromise = Promise.resolve({});
    for (let i = 0; i < 5; ++i) {
      Object.setPrototypeOf(nestedPromise, null);
      nestedPromise = Promise.resolve(nestedPromise);
    }
    content.wrappedJSObject.console.log("oi-test", nestedPromise);
  });

  const node = await waitFor(() => findMessage(hud, "oi-test"));
  const oi = node.querySelector(".tree");
  const [promiseNode] = getObjectInspectorNodes(oi);

  expandObjectInspectorNode(promiseNode);
  await waitFor(() => getObjectInspectorNodes(oi).length > 1);
  checkChildren(promiseNode, [`<state>`, `<value>`]);

  const valueNode = findObjectInspectorNode(oi, "<value>");
  expandObjectInspectorNode(valueNode);
  await waitFor(() => getObjectInspectorChildrenNodes(valueNode).length > 0);
  checkChildren(valueNode, [`<state>`, `<value>`]);
});

add_task(async function testExpandCyclicPromise() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    let resolve;
    const cyclicPromise = new Promise(r => {
      resolve = r;
    });
    Object.setPrototypeOf(cyclicPromise, null);
    const otherPromise = Promise.reject(cyclicPromise);
    otherPromise.catch(() => {});
    Object.setPrototypeOf(otherPromise, null);
    resolve(otherPromise);
    content.wrappedJSObject.console.log("oi-test", cyclicPromise);
  });

  const node = await waitFor(() => findMessage(hud, "oi-test"));
  const oi = node.querySelector(".tree");
  const [promiseNode] = getObjectInspectorNodes(oi);

  expandObjectInspectorNode(promiseNode);
  await waitFor(() => getObjectInspectorNodes(oi).length > 1);
  checkChildren(promiseNode, [`<state>`, `<value>`]);

  const valueNode = findObjectInspectorNode(oi, "<value>");
  expandObjectInspectorNode(valueNode);
  await waitFor(() => getObjectInspectorChildrenNodes(valueNode).length > 0);
  checkChildren(valueNode, [`<state>`, `<reason>`]);

  const reasonNode = findObjectInspectorNode(oi, "<reason>");
  expandObjectInspectorNode(reasonNode);
  await waitFor(() => getObjectInspectorChildrenNodes(reasonNode).length > 0);
  checkChildren(reasonNode, [`<state>`, `<value>`]);
});

function checkChildren(node, expectedChildren) {
  const children = getObjectInspectorChildrenNodes(node);
  is(
    children.length,
    expectedChildren.length,
    "There is the expected number of children"
  );
  children.forEach((child, index) => {
    ok(
      child.textContent.includes(expectedChildren[index]),
      `Expected "${expectedChildren[index]}" child`
    );
  });
}
