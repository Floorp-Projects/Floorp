/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

add_task(async function loadNewChild() {
  const { target, walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  let originalOwnershipSize = 0;
  let longlist = null;
  let firstChild = null;
  const list = await walker.querySelectorAll(walker.rootNode, "#longlist div");
  // Make sure we have the 26 children of longlist in our ownership tree.
  is(list.length, 26, "Expect 26 div children.");
  // Make sure we've read in all those children and incorporated them
  // in our ownership tree.
  const items = await list.items();
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
  firstChild = items[0].actorID;
  // Now get the longlist and release it from the ownership tree.
  const node = await walker.querySelector(walker.rootNode, "#longlist");
  longlist = node.actorID;
  await walker.releaseNode(node);
  // Our ownership size should now be 53 fewer
  // (we forgot about #longlist + 26 children + 26 singleTextChild nodes)
  const newOwnershipSize = await assertOwnershipTrees(walker);
  is(
    newOwnershipSize,
    originalOwnershipSize - 53,
    "Ownership tree should be lower"
  );
  // Now verify that some nodes have gone away
  await checkMissing(target, longlist);
  await checkMissing(target, firstChild);
});
