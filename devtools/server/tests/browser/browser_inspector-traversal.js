/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from inspector-helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

const checkActorIDs = [];

add_task(async function loadNewChild() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  // Make sure that refetching the root document of the walker returns the same
  // actor as the getWalker returned.
  const root = await walker.document();
  ok(
    root === walker.rootNode,
    "Re-fetching the document node should match the root document node."
  );
  checkActorIDs.push(root.actorID);
  await assertOwnershipTrees(walker);
});

add_task(async function testInnerHTML() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  const docElement = await walker.documentElement();
  const longstring = await walker.innerHTML(docElement);
  const innerHTML = await longstring.string();
  const actualInnerHTML = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      return content.document.documentElement.innerHTML;
    }
  );
  ok(innerHTML === actualInnerHTML, "innerHTML should match");
});

add_task(async function testOuterHTML() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  const docElement = await walker.documentElement();
  const longstring = await walker.outerHTML(docElement);
  const outerHTML = await longstring.string();
  const actualOuterHTML = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      return content.document.documentElement.outerHTML;
    }
  );
  ok(outerHTML === actualOuterHTML, "outerHTML should match");
});

add_task(async function testSetOuterHTMLNode() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const newHTML = '<p id="edit-html-done">after edit</p>';
  let node = await walker.querySelector(walker.rootNode, "#edit-html");
  await walker.setOuterHTML(node, newHTML);
  node = await walker.querySelector(walker.rootNode, "#edit-html-done");
  const longstring = await walker.outerHTML(node);
  const outerHTML = await longstring.string();
  is(outerHTML, newHTML, "outerHTML has been updated");
  node = await walker.querySelector(walker.rootNode, "#edit-html");
  ok(!node, "The node with the old ID cannot be selected anymore");
});

add_task(async function testQuerySelector() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  let node = await walker.querySelector(walker.rootNode, "#longlist");
  is(
    node.getAttribute("data-test"),
    "exists",
    "should have found the right node"
  );
  await assertOwnershipTrees(walker);
  node = await walker.querySelector(walker.rootNode, "unknownqueryselector");
  ok(!node, "Should not find a node here.");
  await assertOwnershipTrees(walker);
});

add_task(async function testQuerySelectors() {
  const { target, walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const nodeList = await walker.querySelectorAll(
    walker.rootNode,
    "#longlist div"
  );
  is(nodeList.length, 26, "Expect 26 div children.");
  await assertOwnershipTrees(walker);
  const firstNode = await nodeList.item(0);
  checkActorIDs.push(firstNode.actorID);
  is(firstNode.id, "a", "First child should be a");
  await assertOwnershipTrees(walker);
  let nodes = await nodeList.items();
  is(nodes.length, 26, "Expect 26 nodes");
  is(nodes[0], firstNode, "First node should be reused.");
  ok(nodes[0]._parent, "Parent node should be set.");
  ok(nodes[0]._next || nodes[0]._prev, "Siblings should be set.");
  ok(
    nodes[25]._next || nodes[25]._prev,
    "Siblings of " + nodes[25] + " should be set."
  );
  await assertOwnershipTrees(walker);
  nodes = await nodeList.items(-1);
  is(nodes.length, 1, "Expect 1 node");
  is(nodes[0].id, "z", "Expect it to be the last node.");
  checkActorIDs.push(nodes[0].actorID);
  // Save the node list ID so we can ensure it was destroyed.
  const nodeListID = nodeList.actorID;
  await assertOwnershipTrees(walker);
  await nodeList.release();
  ok(!nodeList.actorID, "Actor should have been destroyed.");
  await assertOwnershipTrees(walker);
  await checkMissing(target, nodeListID);
});

// Helper to check the response of requests that return hasFirst/hasLast/nodes
// node lists (like `children` and `siblings`)
async function checkArray(walker, children, first, last, ids) {
  is(
    children.hasFirst,
    first,
    "Should " + (first ? "" : "not ") + " have the first node."
  );
  is(
    children.hasLast,
    last,
    "Should " + (last ? "" : "not ") + " have the last node."
  );
  is(
    children.nodes.length,
    ids.length,
    "Should have " + ids.length + " children listed."
  );
  let responseIds = "";
  for (const node of children.nodes) {
    responseIds += node.id;
  }
  is(responseIds, ids, "Correct nodes were returned.");
  await assertOwnershipTrees(walker);
}

add_task(async function testNoChildren() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const empty = await walker.querySelector(walker.rootNode, "#empty");
  await assertOwnershipTrees(walker);
  const children = await walker.children(empty);
  await checkArray(walker, children, true, true, "");
});

add_task(async function testLongListTraversal() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const longList = await walker.querySelector(walker.rootNode, "#longlist");
  // First call with no options, expect all children.
  await assertOwnershipTrees(walker);
  let children = await walker.children(longList);
  await checkArray(walker, children, true, true, "abcdefghijklmnopqrstuvwxyz");
  const allChildren = children.nodes;
  await assertOwnershipTrees(walker);
  // maxNodes should limit us to the first 5 nodes.
  await assertOwnershipTrees(walker);
  children = await walker.children(longList, { maxNodes: 5 });
  await checkArray(walker, children, true, false, "abcde");
  await assertOwnershipTrees(walker);
  // maxNodes with the second item centered should still give us the first 5 nodes.
  children = await walker.children(longList, {
    maxNodes: 5,
    center: allChildren[1],
  });
  await checkArray(walker, children, true, false, "abcde");
  // maxNodes with a center in the middle of the list should put that item in the middle
  const center = allChildren[13];
  is(center.id, "n", "Make sure I know how to count letters.");
  children = await walker.children(longList, { maxNodes: 5, center });
  await checkArray(walker, children, false, false, "lmnop");
  // maxNodes with the second-to-last item centered should give us the last 5 nodes.
  children = await walker.children(longList, {
    maxNodes: 5,
    center: allChildren[24],
  });
  await checkArray(walker, children, false, true, "vwxyz");
  // maxNodes with a start in the middle should start at that node and fetch 5
  const start = allChildren[13];
  is(start.id, "n", "Make sure I know how to count letters.");
  children = await walker.children(longList, { maxNodes: 5, start });
  await checkArray(walker, children, false, false, "nopqr");
  // maxNodes near the end should only return what's left
  children = await walker.children(longList, {
    maxNodes: 5,
    start: allChildren[24],
  });
  await checkArray(walker, children, false, true, "yz");
});

add_task(async function testObjectNodeChildren() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const object = await walker.querySelector(walker.rootNode, "object");
  const children = await walker.children(object);
  await checkArray(walker, children, true, true, "1");
});

add_task(async function testNextSibling() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const y = await walker.querySelector(walker.rootNode, "#y");
  is(y.id, "y", "Got the right node.");
  const z = await walker.nextSibling(y);
  is(z.id, "z", "nextSibling got the next node.");
  const nothing = await walker.nextSibling(z);
  is(nothing, null, "nextSibling on the last node returned null.");
});

add_task(async function testPreviousSibling() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const b = await walker.querySelector(walker.rootNode, "#b");
  is(b.id, "b", "Got the right node.");
  const a = await walker.previousSibling(b);
  is(a.id, "a", "nextSibling got the next node.");
  const nothing = await walker.previousSibling(a);
  is(nothing, null, "previousSibling on the first node returned null.");
});

add_task(async function testFrameTraversal() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const childFrame = await walker.querySelector(walker.rootNode, "#childFrame");
  const children = await walker.children(childFrame);
  const nodes = children.nodes;
  is(nodes.length, 1, "There should be only one child of the iframe");
  is(
    nodes[0].nodeType,
    Node.DOCUMENT_NODE,
    "iframe child should be a document node"
  );
  await walker.querySelector(nodes[0], "#z");
});

add_task(async function testLongValue() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  SimpleTest.registerCleanupFunction(async function() {
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
      const { require } = ChromeUtils.import(
        "resource://devtools/shared/loader/Loader.jsm"
      );
      const WalkerActor = require("devtools/server/actors/inspector/walker");
      WalkerActor.setValueSummaryLength(
        WalkerActor.DEFAULT_VALUE_SUMMARY_LENGTH
      );
    });
  });

  const longstringText = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      const { require } = ChromeUtils.import(
        "resource://devtools/shared/loader/Loader.jsm"
      );
      const testSummaryLength = 10;
      const WalkerActor = require("devtools/server/actors/inspector/walker");

      WalkerActor.setValueSummaryLength(testSummaryLength);
      return content.document.getElementById("longstring").firstChild.nodeValue;
    }
  );

  const node = await walker.querySelector(walker.rootNode, "#longstring");
  ok(!node.inlineTextChild, "Text is too long to be inlined");
  // Now we need to get the text node child...
  const children = await walker.children(node, { maxNodes: 1 });
  const textNode = children.nodes[0];
  is(textNode.nodeType, Node.TEXT_NODE, "Value should be a text node");
  const value = await textNode.getNodeValue();
  const valueStr = await value.string();
  is(
    valueStr,
    longstringText,
    "Full node value should match the string from the document."
  );
});

add_task(async function testShortValue() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  const shortstringText = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      return content.document.getElementById("shortstring").firstChild
        .nodeValue;
    }
  );

  const node = await walker.querySelector(walker.rootNode, "#shortstring");
  ok(!!node.inlineTextChild, "Text is short enough to be inlined");
  // Now we need to get the text node child...
  const children = await walker.children(node, { maxNodes: 1 });
  const textNode = children.nodes[0];
  is(textNode.nodeType, Node.TEXT_NODE, "Value should be a text node");
  const value = await textNode.getNodeValue();
  const valueStr = await value.string();
  is(
    valueStr,
    shortstringText,
    "Full node value should match the string from the document."
  );
});

add_task(async function testReleaseWalker() {
  const { target, walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  checkActorIDs.push(walker.actorID);

  await walker.release();
  for (const id of checkActorIDs) {
    await checkMissing(target, id);
  }
});
