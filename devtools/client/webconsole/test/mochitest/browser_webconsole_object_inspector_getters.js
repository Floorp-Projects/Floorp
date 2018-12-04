/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check evaluating and expanding getters in the console.
const TEST_URI = "data:text/html;charset=utf8,<h1>Object Inspector on Getters</h1>";
const {ELLIPSIS} = require("devtools/shared/l10n");

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const LONGSTRING = "ab ".repeat(1e5);

  await ContentTask.spawn(gBrowser.selectedBrowser, LONGSTRING, function(longString) {
    content.wrappedJSObject.console.log(
      "oi-test",
      Object.create(null, Object.getOwnPropertyDescriptors({
        get myStringGetter() {
          return "hello";
        },
        get myNumberGetter() {
          return 123;
        },
        get myUndefinedGetter() {
          return undefined;
        },
        get myNullGetter() {
          return null;
        },
        get myObjectGetter() {
          return {foo: "bar"};
        },
        get myArrayGetter() {
          return Array.from({length: 1000}, (_, i) => i);
        },
        get myMapGetter() {
          return new Map([["foo", {bar: "baz"}]]);
        },
        get myProxyGetter() {
          const handler = {
            get: function(target, name) {
              return name in target ? target[name] : 37;
            },
          };
          return new Proxy({a: 1}, handler);
        },
        get myThrowingGetter() {
          throw new Error("myError");
        },
        get myLongStringGetter() {
          return longString;
        },
      }))
    );
  });

  const node = await waitFor(() => findMessage(hud, "oi-test"));
  const oi = node.querySelector(".tree");

  expandNode(oi);
  await waitFor(() => getOiNodes(oi).length > 1);

  await testStringGetter(oi);
  await testNumberGetter(oi);
  await testUndefinedGetter(oi);
  await testNullGetter(oi);
  await testObjectGetter(oi);
  await testArrayGetter(oi);
  await testMapGetter(oi);
  await testProxyGetter(oi);
  await testThrowingGetter(oi);
  await testLongStringGetter(oi, LONGSTRING);
});

async function testStringGetter(oi) {
  let node = getOiNode(oi, "myStringGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myStringGetter")));

  node = getOiNode(oi, "myStringGetter");
  ok(node.textContent.includes(`myStringGetter: "hello"`),
    "String getter now has the expected text content");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
}

async function testNumberGetter(oi) {
  let node = getOiNode(oi, "myNumberGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myNumberGetter")));

  node = getOiNode(oi, "myNumberGetter");
  ok(node.textContent.includes(`myNumberGetter: 123`),
    "Number getter now has the expected text content");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
}

async function testUndefinedGetter(oi) {
  let node = getOiNode(oi, "myUndefinedGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myUndefinedGetter")));

  node = getOiNode(oi, "myUndefinedGetter");
  ok(node.textContent.includes(`myUndefinedGetter: undefined`),
    "undefined getter now has the expected text content");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
}

async function testNullGetter(oi) {
  let node = getOiNode(oi, "myNullGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myNullGetter")));

  node = getOiNode(oi, "myNullGetter");
  ok(node.textContent.includes(`myNullGetter: null`),
    "null getter now has the expected text content");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
}

async function testObjectGetter(oi) {
  let node = getOiNode(oi, "myObjectGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myObjectGetter")));

  node = getOiNode(oi, "myObjectGetter");
  ok(node.textContent.includes(`myObjectGetter: Object { foo: "bar" }`),
    "object getter now has the expected text content");
  is(nodeIsExpandable(node), true, "The node can be expanded");

  expandNode(node);
  await waitFor(() => getChildrenNodes(node).length > 0);
  checkChildren(node, [`foo: "bar"`, `<prototype>`]);
}

async function testArrayGetter(oi) {
  let node = getOiNode(oi, "myArrayGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myArrayGetter")));

  node = getOiNode(oi, "myArrayGetter");
  ok(node.textContent.includes(
    `myArrayGetter: Array(1000) [ 0, 1, 2, ${ELLIPSIS} ]`),
    "Array getter now has the expected text content - ");
  is(nodeIsExpandable(node), true, "The node can be expanded");

  expandNode(node);
  await waitFor(() => getChildrenNodes(node).length > 0);
  const children = getChildrenNodes(node);

  const firstBucket = children[0];
  ok(firstBucket.textContent.includes(`[0${ELLIPSIS}99]`), "Array has buckets");

  is(nodeIsExpandable(firstBucket), true, "The bucket can be expanded");
  expandNode(firstBucket);
  await waitFor(() => getChildrenNodes(firstBucket).length > 0);
  checkChildren(firstBucket, Array.from({length: 100}, (_, i) => `${i}: ${i}`));
}

async function testMapGetter(oi) {
  let node = getOiNode(oi, "myMapGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myMapGetter")));

  node = getOiNode(oi, "myMapGetter");
  ok(node.textContent.includes(`myMapGetter: Map`),
    "map getter now has the expected text content");
  is(nodeIsExpandable(node), true, "The node can be expanded");

  expandNode(node);
  await waitFor(() => getChildrenNodes(node).length > 0);
  checkChildren(node, [`size`, `<entries>`, `<prototype>`]);

  const entriesNode = getOiNode(oi, "<entries>");
  expandNode(entriesNode);
  await waitFor(() => getChildrenNodes(entriesNode).length > 0);
  // "→" character. Need to be instantiated this way for the test to pass.
  const arrow = String.fromCodePoint(8594);
  checkChildren(entriesNode, [`foo ${arrow} Object { ${ELLIPSIS} }`]);

  const entryNode = getChildrenNodes(entriesNode)[0];
  expandNode(entryNode);
  await waitFor(() => getChildrenNodes(entryNode).length > 0);
  checkChildren(entryNode, [`<key>: "foo"`, `<value>: Object { ${ELLIPSIS} }`]);
}

async function testProxyGetter(oi) {
  let node = getOiNode(oi, "myProxyGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myProxyGetter")));

  node = getOiNode(oi, "myProxyGetter");
  ok(node.textContent.includes(`myProxyGetter: Proxy`),
    "proxy getter now has the expected text content");
  is(nodeIsExpandable(node), true, "The node can be expanded");

  expandNode(node);
  await waitFor(() => getChildrenNodes(node).length > 0);
  checkChildren(node, [`<target>`, `<handler>`]);

  const targetNode = getOiNode(oi, "<target>");
  expandNode(targetNode);
  await waitFor(() => getChildrenNodes(targetNode).length > 0);
  checkChildren(targetNode, [`a: 1`, `<prototype>`]);

  const handlerNode = getOiNode(oi, "<handler>");
  expandNode(handlerNode);
  await waitFor(() => getChildrenNodes(handlerNode).length > 0);
  checkChildren(handlerNode, [`get:`, `<prototype>`]);
}

async function testThrowingGetter(oi) {
  let node = getOiNode(oi, "myThrowingGetter");
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() => !getInvokeGetterButton(getOiNode(oi, "myThrowingGetter")));

  node = getOiNode(oi, "myThrowingGetter");
  ok(node.textContent.includes(`myThrowingGetter: Error: "myError"`),
    "throwing getter does show the error");
  is(nodeIsExpandable(node), true, "The node can be expanded");

  expandNode(node);
  await waitFor(() => getChildrenNodes(node).length > 0);
  checkChildren(node,
    [`columnNumber`, `fileName`, `lineNumber`, `message`, `stack`, `<prototype>`]);
}

async function testLongStringGetter(oi, longString) {
  const getLongStringNode = () => getOiNode(oi, "myLongStringGetter");
  const node = getLongStringNode();
  is(nodeIsExpandable(node), false, "The node can't be expanded");
  const invokeButton = getInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() =>
    getLongStringNode().textContent.includes(`myLongStringGetter: "ab ab`));
  ok(true, "longstring getter shows the initial text");
  is(nodeIsExpandable(getLongStringNode()), true, "The node can be expanded");

  expandNode(getLongStringNode());
  await waitFor(() =>
    getLongStringNode().textContent.includes(`myLongStringGetter: "${longString}"`));
  ok(true, "the longstring was expanded");
}

function checkChildren(node, expectedChildren) {
  const children = getChildrenNodes(node);
  is(children.length, expectedChildren.length,
    "There is the expected number of children");
  children.forEach((child, index) => {
    ok(child.textContent.includes(expectedChildren[index]),
      `Expected "${expectedChildren[index]}" child`);
  });
}

function nodeIsExpandable(node) {
  return !!getNodeArrow(node);
}

function expandNode(node) {
  const arrow = getNodeArrow(node);
  if (!arrow) {
    ok(false, "Node can't be expanded");
    return;
  }
  arrow.click();
}

function getNodeArrow(node) {
  return node.querySelector(".arrow");
}

function getOiNodes(oi) {
  return oi.querySelectorAll(".tree-node");
}

function getChildrenNodes(node) {
  const getLevel = n => parseInt(n.getAttribute("aria-level"), 10);
  const level = getLevel(node);
  const childLevel = level + 1;
  const children = [];

  let currentNode = node;
  while (currentNode.nextSibling && getLevel(currentNode.nextSibling) === childLevel) {
    currentNode = currentNode.nextSibling;
    children.push(currentNode);
  }

  return children;
}

function getInvokeGetterButton(node) {
  return node.querySelector(".invoke-getter");
}

function getOiNode(oi, nodeLabel) {
  return [...oi.querySelectorAll(".tree-node")].find(node => {
    const label = node.querySelector(".object-label");
    if (!label) {
      return false;
    }
    return label.textContent === nodeLabel;
  });
}
