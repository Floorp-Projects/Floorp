/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check evaluating and expanding getters in the Browser Console.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>Object Inspector on Getters</h1>";
const { ELLIPSIS } = require("resource://devtools/shared/l10n.js");

add_task(async function () {
  // Show the content messages
  await pushPref("devtools.browsertoolbox.scope", "everything");

  await addTab(TEST_URI);

  info("Open the Browser Console");
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  const LONGSTRING = "ab ".repeat(1e5);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [LONGSTRING],
    function (longString) {
      const obj = Object.create(
        null,
        Object.getOwnPropertyDescriptors({
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
          get myZeroGetter() {
            return 0;
          },
          get myEmptyStringGetter() {
            return "";
          },
          get myFalseGetter() {
            return false;
          },
          get myTrueGetter() {
            return true;
          },
          get myObjectGetter() {
            return { foo: "bar" };
          },
          get myArrayGetter() {
            return Array.from({ length: 1000 }, (_, i) => i);
          },
          get myMapGetter() {
            return new Map([["foo", { bar: "baz" }]]);
          },
          get myProxyGetter() {
            const handler = {
              get(target, name) {
                return name in target ? target[name] : 37;
              },
            };
            return new Proxy({ a: 1 }, handler);
          },
          get myThrowingGetter() {
            throw new Error("myError");
          },
          get myLongStringGetter() {
            return longString;
          },
        })
      );
      Object.defineProperty(obj, "MyPrint", { get: content.print });
      Object.defineProperty(obj, "MyElement", { get: content.Element });
      Object.defineProperty(obj, "MySetAttribute", {
        get: content.Element.prototype.setAttribute,
      });
      Object.defineProperty(obj, "MySetClassName", {
        get: Object.getOwnPropertyDescriptor(
          content.Element.prototype,
          "className"
        ).set,
      });

      content.wrappedJSObject.console.log("oi-test", obj);
    }
  );

  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-test"));
  const oi = node.querySelector(".tree");

  expandObjectInspectorNode(oi);
  await waitFor(() => getObjectInspectorNodes(oi).length > 1);

  await testStringGetter(oi);
  await testNumberGetter(oi);
  await testUndefinedGetter(oi);
  await testNullGetter(oi);
  await testZeroGetter(oi);
  await testEmptyStringGetter(oi);
  await testFalseGetter(oi);
  await testTrueGetter(oi);
  await testObjectGetter(oi);
  await testArrayGetter(oi);
  await testMapGetter(oi);
  await testProxyGetter(oi);
  await testThrowingGetter(oi);
  await testLongStringGetter(oi, LONGSTRING);
  await testUnsafeGetters(oi);
});

async function testStringGetter(oi) {
  let node = findObjectInspectorNode(oi, "myStringGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myStringGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myStringGetter");
  ok(
    node.textContent.includes(`myStringGetter: "hello"`),
    "String getter now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
}

async function testNumberGetter(oi) {
  let node = findObjectInspectorNode(oi, "myNumberGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myNumberGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myNumberGetter");
  ok(
    node.textContent.includes(`myNumberGetter: 123`),
    "Number getter now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
}

async function testUndefinedGetter(oi) {
  let node = findObjectInspectorNode(oi, "myUndefinedGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myUndefinedGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myUndefinedGetter");
  ok(
    node.textContent.includes(`myUndefinedGetter: undefined`),
    "undefined getter now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
}

async function testNullGetter(oi) {
  let node = findObjectInspectorNode(oi, "myNullGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myNullGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myNullGetter");
  ok(
    node.textContent.includes(`myNullGetter: null`),
    "null getter now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
}

async function testZeroGetter(oi) {
  let node = findObjectInspectorNode(oi, "myZeroGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myZeroGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myZeroGetter");
  ok(
    node.textContent.includes(`myZeroGetter: 0`),
    "0 getter now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
}

async function testEmptyStringGetter(oi) {
  let node = findObjectInspectorNode(oi, "myEmptyStringGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myEmptyStringGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myEmptyStringGetter");
  ok(
    node.textContent.includes(`myEmptyStringGetter: ""`),
    "empty string getter now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
}

async function testFalseGetter(oi) {
  let node = findObjectInspectorNode(oi, "myFalseGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myFalseGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myFalseGetter");
  ok(
    node.textContent.includes(`myFalseGetter: false`),
    "false getter now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
}

async function testTrueGetter(oi) {
  let node = findObjectInspectorNode(oi, "myTrueGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myTrueGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myTrueGetter");
  ok(
    node.textContent.includes(`myTrueGetter: true`),
    "false getter now has the expected text content"
  );
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
}

async function testObjectGetter(oi) {
  let node = findObjectInspectorNode(oi, "myObjectGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myObjectGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myObjectGetter");
  ok(
    node.textContent.includes(`myObjectGetter: Object { foo: "bar" }`),
    "object getter now has the expected text content"
  );
  is(isObjectInspectorNodeExpandable(node), true, "The node can be expanded");

  expandObjectInspectorNode(node);
  await waitFor(() => !!getObjectInspectorChildrenNodes(node).length);
  checkChildren(node, [`foo: "bar"`, `<prototype>`]);
}

async function testArrayGetter(oi) {
  let node = findObjectInspectorNode(oi, "myArrayGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myArrayGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myArrayGetter");
  ok(
    node.textContent.includes(
      `myArrayGetter: Array(1000) [ 0, 1, 2, ${ELLIPSIS} ]`
    ),
    "Array getter now has the expected text content - "
  );
  is(isObjectInspectorNodeExpandable(node), true, "The node can be expanded");

  expandObjectInspectorNode(node);
  await waitFor(() => !!getObjectInspectorChildrenNodes(node).length);
  const children = getObjectInspectorChildrenNodes(node);

  const firstBucket = children[0];
  ok(firstBucket.textContent.includes(`[0${ELLIPSIS}99]`), "Array has buckets");

  is(
    isObjectInspectorNodeExpandable(firstBucket),
    true,
    "The bucket can be expanded"
  );
  expandObjectInspectorNode(firstBucket);
  await waitFor(() => !!getObjectInspectorChildrenNodes(firstBucket).length);
  checkChildren(
    firstBucket,
    Array.from({ length: 100 }, (_, i) => `${i}: ${i}`)
  );
}

async function testMapGetter(oi) {
  let node = findObjectInspectorNode(oi, "myMapGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myMapGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myMapGetter");
  ok(
    node.textContent.includes(`myMapGetter: Map`),
    "map getter now has the expected text content"
  );
  is(isObjectInspectorNodeExpandable(node), true, "The node can be expanded");

  expandObjectInspectorNode(node);
  await waitFor(() => !!getObjectInspectorChildrenNodes(node).length);
  checkChildren(node, [`size`, `<entries>`, `<prototype>`]);

  const entriesNode = findObjectInspectorNode(oi, "<entries>");
  expandObjectInspectorNode(entriesNode);
  await waitFor(() => !!getObjectInspectorChildrenNodes(entriesNode).length);
  checkChildren(entriesNode, [`foo → Object { bar: "baz" }`]);

  const entryNode = getObjectInspectorChildrenNodes(entriesNode)[0];
  expandObjectInspectorNode(entryNode);
  await waitFor(() => !!getObjectInspectorChildrenNodes(entryNode).length);
  checkChildren(entryNode, [`<key>: "foo"`, `<value>: Object { bar: "baz" }`]);
}

async function testProxyGetter(oi) {
  let node = findObjectInspectorNode(oi, "myProxyGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myProxyGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myProxyGetter");
  ok(
    node.textContent.includes(`myProxyGetter: Proxy`),
    "proxy getter now has the expected text content"
  );
  is(isObjectInspectorNodeExpandable(node), true, "The node can be expanded");

  expandObjectInspectorNode(node);
  await waitFor(() => !!getObjectInspectorChildrenNodes(node).length);
  checkChildren(node, [`<target>`, `<handler>`]);

  const targetNode = findObjectInspectorNode(oi, "<target>");
  expandObjectInspectorNode(targetNode);
  await waitFor(() => !!getObjectInspectorChildrenNodes(targetNode).length);
  checkChildren(targetNode, [`a: 1`, `<prototype>`]);

  const handlerNode = findObjectInspectorNode(oi, "<handler>");
  expandObjectInspectorNode(handlerNode);
  await waitFor(() => !!getObjectInspectorChildrenNodes(handlerNode).length);
  checkChildren(handlerNode, [`get:`, `<prototype>`]);
}

async function testThrowingGetter(oi) {
  let node = findObjectInspectorNode(oi, "myThrowingGetter");
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "myThrowingGetter")
      )
  );

  node = findObjectInspectorNode(oi, "myThrowingGetter");
  ok(
    node.textContent.includes(`myThrowingGetter: Error`),
    "throwing getter does show the error"
  );
  is(isObjectInspectorNodeExpandable(node), true, "The node can be expanded");

  expandObjectInspectorNode(node);
  await waitFor(() => !!getObjectInspectorChildrenNodes(node).length);
  checkChildren(node, [
    `columnNumber`,
    `fileName`,
    `lineNumber`,
    `message`,
    `stack`,
    `<prototype>`,
  ]);
}

async function testLongStringGetter(oi, longString) {
  const getLongStringNode = () =>
    findObjectInspectorNode(oi, "myLongStringGetter");
  const node = getLongStringNode();
  is(
    isObjectInspectorNodeExpandable(node),
    false,
    "The node can't be expanded"
  );
  const invokeButton = getObjectInspectorInvokeGetterButton(node);
  ok(invokeButton, "There is an invoke button as expected");

  invokeButton.click();
  await waitFor(() =>
    getLongStringNode().textContent.includes(`myLongStringGetter: "ab ab`)
  );
  ok(true, "longstring getter shows the initial text");
  is(
    isObjectInspectorNodeExpandable(getLongStringNode()),
    true,
    "The node can be expanded"
  );

  expandObjectInspectorNode(getLongStringNode());
  await waitFor(() =>
    getLongStringNode().textContent.includes(
      `myLongStringGetter: "${longString}"`
    )
  );
  ok(true, "the longstring was expanded");
}

async function testUnsafeGetters(oi) {
  const props = [
    [
      "MyPrint",
      "MyPrint: TypeError: 'print' called on an object that does not implement interface Window.",
    ],
    ["MyElement", "MyElement: TypeError: Illegal constructor."],
    [
      "MySetAttribute",
      "MySetAttribute: TypeError: 'setAttribute' called on an object that does not implement interface Element.",
    ],
    [
      "MySetClassName",
      "MySetClassName: TypeError: 'set className' called on an object that does not implement interface Element.",
    ],
  ];

  for (const [name, text] of props) {
    const getNode = () => findObjectInspectorNode(oi, name);
    is(
      isObjectInspectorNodeExpandable(getNode()),
      false,
      `The ${name} node can't be expanded`
    );
    const invokeButton = getObjectInspectorInvokeGetterButton(getNode());
    ok(invokeButton, `There is an invoke button for ${name} as expected`);

    invokeButton.click();
    await waitFor(() => getNode().textContent.includes(text));
    ok(true, `${name} getter shows the error message ${text}`);
  }
}

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
      `Expected "${child.textContent}" to include "${expectedChildren[index]}"`
    );
  });
}
