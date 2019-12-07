/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  await testRearrange(walker);
  await testInsertInvalidInput(walker);
});

async function testRearrange(walker) {
  const longlist = await walker.querySelector(walker.rootNode, "#longlist");
  let children = await walker.children(longlist);
  const nodeA = children.nodes[0];
  is(nodeA.id, "a", "Got the expected node.");

  // Move nodeA to the end of the list.
  await walker.insertBefore(nodeA, longlist, null);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    ok(
      !content.document.querySelector("#a").nextSibling,
      "a should now be at the end of the list."
    );
  });

  children = await walker.children(longlist);
  is(
    nodeA,
    children.nodes[children.nodes.length - 1],
    "a should now be the last returned child."
  );

  // Now move it to the middle of the list.
  const nextNode = children.nodes[13];
  await walker.insertBefore(nodeA, longlist, nextNode);

  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    [nextNode.actorID],
    async function(actorID) {
      const { require } = ChromeUtils.import(
        "resource://devtools/shared/Loader.jsm"
      );
      const { DebuggerServer } = require("devtools/server/debugger-server");
      const {
        DocumentWalker,
      } = require("devtools/server/actors/inspector/document-walker");
      const sibling = new DocumentWalker(
        content.document.querySelector("#a"),
        content
      ).nextSibling();
      // Convert actorID to current compartment string otherwise
      // searchAllConnectionsForActor is confused and won't find the actor.
      actorID = String(actorID);
      const nodeActor = DebuggerServer.searchAllConnectionsForActor(actorID);
      is(
        sibling,
        nodeActor.rawNode,
        "Node should match the expected next node."
      );
    }
  );

  children = await walker.children(longlist);
  is(nodeA, children.nodes[13], "a should be where we expect it.");
  is(nextNode, children.nodes[14], "next node should be where we expect it.");
}

async function testInsertInvalidInput(walker) {
  const longlist = await walker.querySelector(walker.rootNode, "#longlist");
  const children = await walker.children(longlist);
  const nodeA = children.nodes[0];
  const nextSibling = children.nodes[1];

  // Now move it to the original location and make sure no mutation happens.
  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    [longlist.actorID],
    async function(actorID) {
      const { require } = ChromeUtils.import(
        "resource://devtools/shared/Loader.jsm"
      );
      const { DebuggerServer } = require("devtools/server/debugger-server");
      // Convert actorID to current compartment string otherwise
      // searchAllConnectionsForActor is confused and won't find the actor.
      actorID = String(actorID);
      const nodeActor = DebuggerServer.searchAllConnectionsForActor(actorID);
      content.hasMutated = false;
      content.observer = new content.MutationObserver(() => {
        content.hasMutated = true;
      });
      content.observer.observe(nodeActor.rawNode, {
        childList: true,
      });
    }
  );

  await walker.insertBefore(nodeA, longlist, nodeA);
  let hasMutated = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    async function() {
      const state = content.hasMutated;
      content.hasMutated = false;
      return state;
    }
  );
  ok(!hasMutated, "hasn't mutated");

  await walker.insertBefore(nodeA, longlist, nextSibling);
  hasMutated = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    async function() {
      const state = content.hasMutated;
      content.hasMutated = false;
      return state;
    }
  );
  ok(!hasMutated, "still hasn't mutated after inserting before nextSibling");

  await walker.insertBefore(nodeA, longlist);
  hasMutated = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    async function() {
      const state = content.hasMutated;
      content.hasMutated = false;
      return state;
    }
  );
  ok(hasMutated, "has mutated after inserting with null sibling");

  await walker.insertBefore(nodeA, longlist);
  hasMutated = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    async function() {
      const state = content.hasMutated;
      content.hasMutated = false;
      return state;
    }
  );
  ok(!hasMutated, "hasn't mutated after inserting with null sibling again");

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    content.observer.disconnect();
  });
}
