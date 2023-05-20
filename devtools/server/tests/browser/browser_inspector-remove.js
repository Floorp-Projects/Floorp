/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

add_task(async function testRemoveSubtree() {
  const { target, walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    function ignoreNode(node) {
      // Duplicate the walker logic to skip blank nodes...
      return (
        node.nodeType === content.Node.TEXT_NODE &&
        !/[^\s]/.test(node.nodeValue)
      );
    }

    let nextSibling = content.document.querySelector("#longlist").nextSibling;
    while (nextSibling && ignoreNode(nextSibling)) {
      nextSibling = nextSibling.nextSibling;
    }

    let previousSibling =
      content.document.querySelector("#longlist").previousSibling;
    while (previousSibling && ignoreNode(previousSibling)) {
      previousSibling = previousSibling.previousSibling;
    }
    content.nextSibling = nextSibling;
    content.previousSibling = previousSibling;
  });

  let originalOwnershipSize = 0;
  const longlist = await walker.querySelector(walker.rootNode, "#longlist");
  const longlistID = longlist.actorID;
  await walker.children(longlist);
  originalOwnershipSize = await assertOwnershipTrees(walker);
  // Here is how the ownership tree is summed up:
  // #document                      1
  //   <html>                       1
  //     <body>                     1
  //       <div id=longlist>        1
  //         <div id=a>a</div>   26*2 (each child plus it's singleTextChild)
  //         ...
  //         <div id=z>z</div>
  //                             -----
  //                               56
  is(originalOwnershipSize, 56, "Correct number of items in ownership tree");

  const onMutation = waitForMutation(walker, isChildList);
  const siblings = await walker.removeNode(longlist);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[siblings.previousSibling.actorID, siblings.nextSibling.actorID]],
    function ([previousActorID, nextActorID]) {
      const { require } = ChromeUtils.importESModule(
        "resource://devtools/shared/loader/Loader.sys.mjs"
      );
      const {
        DevToolsServer,
      } = require("resource://devtools/server/devtools-server.js");

      // Convert actorID to current compartment string otherwise
      // searchAllConnectionsForActor is confused and won't find the actor.
      previousActorID = String(previousActorID);
      nextActorID = String(nextActorID);
      const previous =
        DevToolsServer.searchAllConnectionsForActor(previousActorID);
      const next = DevToolsServer.searchAllConnectionsForActor(nextActorID);

      is(
        previous.rawNode,
        content.previousSibling,
        "Should have returned the previous sibling."
      );
      is(
        next.rawNode,
        content.nextSibling,
        "Should have returned the next sibling."
      );
    }
  );
  await onMutation;
  // Our ownership size should now be 51 fewer (we forgot about #longlist + 26
  // children + 26 singleTextChild nodes, but learned about #longlist's
  // prev/next sibling)
  const newOwnershipSize = await assertOwnershipTrees(walker);
  is(
    newOwnershipSize,
    originalOwnershipSize - 51,
    "Ownership tree should be lower"
  );
  // Now verify that some nodes have gone away
  return checkMissing(target, longlistID);
});
