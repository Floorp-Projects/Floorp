/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: [2, {"vars": "local"}] */

/* import-globals-from head.js */

"use strict";

async function checkTreeFromRootSelector(tree, selector, inspector) {
  let {markup} = inspector;

  info(`Find and expand the shadow DOM host matching selector ${selector}.`);
  let rootFront = await getNodeFront(selector, inspector);
  let rootContainer = markup.getContainer(rootFront);

  let parsedTree = parseTree(tree);
  let treeRoot = parsedTree.children[0];
  await checkNode(treeRoot, rootContainer, inspector);
}

async function checkNode(treeNode, container, inspector) {
  let {node, children, path} = treeNode;
  info("Checking [" + path + "]");
  info("Checking node: " + node);

  let slotted = node.includes("!slotted");
  if (slotted) {
    checkSlotted(container, node.replace("!slotted", ""));
  } else {
    checkText(container, node);
  }

  if (!children.length) {
    ok(!container.canExpand, "Container for [" + path + "] has no children");
    return;
  }

  // Expand the container if not already done.
  if (!container.expanded) {
    await expandContainer(inspector, container);
  }

  let containers = container.getChildContainers();
  is(containers.length, children.length,
     "Node [" + path + "] has the expected number of children");
  for (let i = 0; i < children.length; i++) {
    await checkNode(children[i], containers[i], inspector);
  }
}

/**
 * Helper designed to parse a tree represented as:
 * root
 *   child1
 *     subchild1
 *     subchild2
 *   child2
 *     subchild3!slotted
 *
 * Lines represent a simplified view of the markup, where the trimmed line is supposed to
 * be included in the text content of the actual markupview container.
 * This method returns an object that can be passed to checkNode() to verify the current
 * markup view displays the expected structure.
 */
function parseTree(inputString) {
  let tree = {
    level: 0,
    children: []
  };
  let lines = inputString.split("\n");
  lines = lines.filter(l => l.trim());

  let currentNode = tree;
  for (let line of lines) {
    let nodeString = line.trim();
    let level = line.split("  ").length;

    let parent;
    if (level > currentNode.level) {
      parent = currentNode;
    } else {
      parent = currentNode.parent;
      for (let i = 0; i < currentNode.level - level; i++) {
        parent = parent.parent;
      }
    }

    let node = {
      node: nodeString,
      children: [],
      parent,
      level,
      path: parent.path + " " + nodeString
    };

    parent.children.push(node);
    currentNode = node;
  }

  return tree;
}

function checkSlotted(container, expectedType = "div") {
  checkText(container, expectedType);
  ok(container.isSlotted(), "Container is a slotted container");
  ok(container.elt.querySelector(".reveal-link"),
     "Slotted container has a reveal link element");
}

function checkText(container, expectedText) {
  let textContent = container.elt.textContent;
  ok(textContent.includes(expectedText), "Container has expected text: " + expectedText);
}

function waitForMutation(inspector, type) {
  return waitForNMutations(inspector, type, 1);
}

function waitForNMutations(inspector, type, count) {
  info(`Expecting ${count} markupmutation of type ${type}`);
  let receivedMutations = 0;
  return new Promise(resolve => {
    inspector.on("markupmutation", function onMutation(mutations) {
      let validMutations = mutations.filter(m => m.type === type).length;
      receivedMutations = receivedMutations + validMutations;
      if (receivedMutations == count) {
        inspector.off("markupmutation", onMutation);
        resolve();
      }
    });
  });
}

/**
 * Temporarily flip all the preferences needed to enable web components.
 */
async function enableWebComponents() {
  await pushPref("dom.webcomponents.shadowdom.enabled", true);
  await pushPref("dom.webcomponents.customelements.enabled", true);
}
