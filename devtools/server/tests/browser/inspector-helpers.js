/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported assertOwnershipTrees, checkMissing, waitForMutation,
   isSrcChange, isUnretained, isChildList */

function serverOwnershipTree(walkerArg) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[walkerArg.actorID]],
    function (actorID) {
      const { require } = ChromeUtils.importESModule(
        "resource://devtools/shared/loader/Loader.sys.mjs"
      );
      const {
        DevToolsServer,
      } = require("resource://devtools/server/devtools-server.js");
      const {
        DocumentWalker,
      } = require("resource://devtools/server/actors/inspector/document-walker.js");

      // Convert actorID to current compartment string otherwise
      // searchAllConnectionsForActor is confused and won't find the actor.
      actorID = String(actorID);
      const serverWalker = DevToolsServer.searchAllConnectionsForActor(actorID);

      function sortOwnershipChildrenContentScript(children) {
        return children.sort((a, b) => a.name.localeCompare(b.name));
      }

      function serverOwnershipSubtree(walker, node) {
        const actor = walker.getNode(node);
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
async function checkMissing({ client }, actorID) {
  const front = client.getFrontByID(actorID);
  ok(
    !front,
    "Front shouldn't be accessible from the client for actorID: " + actorID
  );

  try {
    await client.request({
      to: actorID,
      type: "request",
    });
    ok(false, "The actor wasn't missing as the request worked");
  } catch (e) {
    is(
      e.error,
      "noSuchActor",
      "node list actor should no longer be contactable."
    );
  }
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

function isSrcChange(change) {
  return change.type === "attributes" && change.attributeName === "src";
}

function isUnretained(change) {
  return change.type === "unretained";
}

function isChildList(change) {
  return change.type === "childList";
}
