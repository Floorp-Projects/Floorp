/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported assertOwnershipTrees, checkMissing, waitForMutation,
   isSrcChange, isUnretained, isNewRoot,
   assertSrcChange, assertUnload, assertFrameLoad, assertChildList,
*/

function serverOwnershipTree(walkerArg) {
  return ContentTask.spawn(
    gBrowser.selectedBrowser,
    [walkerArg.actorID],
    function(actorID) {
      const { require } = ChromeUtils.import(
        "resource://devtools/shared/Loader.jsm"
      );
      const { DebuggerServer } = require("devtools/server/main");
      const {
        DocumentWalker,
      } = require("devtools/server/actors/inspector/document-walker");

      // Convert actorID to current compartment string otherwise
      // searchAllConnectionsForActor is confused and won't find the actor.
      actorID = String(actorID);
      const serverWalker = DebuggerServer.searchAllConnectionsForActor(actorID);

      function sortOwnershipChildrenContentScript(children) {
        return children.sort((a, b) => a.name.localeCompare(b.name));
      }

      function serverOwnershipSubtree(walker, node) {
        const actor = walker._refMap.get(node);
        if (!actor) {
          return undefined;
        }

        const children = [];
        const docwalker = new DocumentWalker(node, content);
        let child = docwalker.firstChild();
        while (child) {
          const item = serverOwnershipSubtree(walker, child);
          if (item) {
            children.push(item);
          }
          child = docwalker.nextSibling();
        }
        return {
          name: actor.actorID,
          children: sortOwnershipChildrenContentScript(children),
        };
      }
      return {
        root: serverOwnershipSubtree(serverWalker, serverWalker.rootDoc),
        orphaned: [...serverWalker._orphaned].map(o =>
          serverOwnershipSubtree(serverWalker, o.rawNode)
        ),
        retained: [...serverWalker._retainedOrphans].map(o =>
          serverOwnershipSubtree(serverWalker, o.rawNode)
        ),
      };
    }
  );
}

function sortOwnershipChildren(children) {
  return children.sort((a, b) => a.name.localeCompare(b.name));
}

function clientOwnershipSubtree(node) {
  return {
    name: node.actorID,
    children: sortOwnershipChildren(
      node.treeChildren().map(child => clientOwnershipSubtree(child))
    ),
  };
}

function clientOwnershipTree(walker) {
  return {
    root: clientOwnershipSubtree(walker.rootNode),
    orphaned: [...walker._orphaned].map(o => clientOwnershipSubtree(o)),
    retained: [...walker._retainedOrphans].map(o => clientOwnershipSubtree(o)),
  };
}

function ownershipTreeSize(tree) {
  let size = 1;
  for (const child of tree.children) {
    size += ownershipTreeSize(child);
  }
  return size;
}

async function assertOwnershipTrees(walker) {
  const serverTree = await serverOwnershipTree(walker);
  const clientTree = clientOwnershipTree(walker);
  is(
    JSON.stringify(clientTree, null, " "),
    JSON.stringify(serverTree, null, " "),
    "Server and client ownership trees should match."
  );

  return ownershipTreeSize(clientTree.root);
}

// Verify that an actorID is inaccessible both from the client library and the server.
function checkMissing({ client }, actorID) {
  return new Promise(resolve => {
    const front = client.getFrontByID(actorID);
    ok(
      !front,
      "Front shouldn't be accessible from the client for actorID: " + actorID
    );

    client
      .request(
        {
          to: actorID,
          type: "request",
        },
        response => {
          is(
            response.error,
            "noSuchActor",
            "node list actor should no longer be contactable."
          );
          resolve(undefined);
        }
      )
      .catch(() => {});
  });
}

// Load mutations aren't predictable, so keep accumulating mutations until
// the one we're looking for shows up.
function waitForMutation(walker, test, mutations = []) {
  return new Promise(resolve => {
    for (const change of mutations) {
      if (test(change)) {
        resolve(mutations);
      }
    }

    walker.once("mutations", newMutations => {
      waitForMutation(walker, test, mutations.concat(newMutations)).then(
        finalMutations => {
          resolve(finalMutations);
        }
      );
    });
  });
}

function assertAndStrip(mutations, message, test) {
  const size = mutations.length;
  mutations = mutations.filter(test);
  ok(mutations.size != size, message);
  return mutations;
}

function isSrcChange(change) {
  return change.type === "attributes" && change.attributeName === "src";
}

function isUnload(change) {
  return change.type === "documentUnload";
}

function isFrameLoad(change) {
  return change.type === "frameLoad";
}

function isUnretained(change) {
  return change.type === "unretained";
}

function isChildList(change) {
  return change.type === "childList";
}

function isNewRoot(change) {
  return change.type === "newRoot";
}

// Make sure an iframe's src attribute changed and then
// strip that mutation out of the list.
function assertSrcChange(mutations) {
  return assertAndStrip(
    mutations,
    "Should have had an iframe source change.",
    isSrcChange
  );
}

// Make sure there's an unload in the mutation list and strip
// that mutation out of the list
function assertUnload(mutations) {
  return assertAndStrip(
    mutations,
    "Should have had a document unload change.",
    isUnload
  );
}

// Make sure there's a frame load in the mutation list and strip
// that mutation out of the list
function assertFrameLoad(mutations) {
  return assertAndStrip(
    mutations,
    "Should have had a frame load change.",
    isFrameLoad
  );
}

// Make sure there's a childList change in the mutation list and strip
// that mutation out of the list
function assertChildList(mutations) {
  return assertAndStrip(
    mutations,
    "Should have had a frame load change.",
    isChildList
  );
}
