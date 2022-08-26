/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from inspector-helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

function loadSelector(walker, selector) {
  return walker.querySelectorAll(walker.rootNode, selector).then(nodeList => {
    return nodeList.items();
  });
}

function loadSelectors(walker, selectors) {
  return Promise.all(Array.from(selectors, sel => loadSelector(walker, sel)));
}

function doMoves(movesArg) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [movesArg], function(
    moves
  ) {
    function setParent(nodeSelector, newParentSelector) {
      const node = content.document.querySelector(nodeSelector);
      if (newParentSelector) {
        const newParent = content.document.querySelector(newParentSelector);
        newParent.appendChild(node);
      } else {
        node.remove();
      }
    }
    for (const move of moves) {
      setParent(move[0], move[1]);
    }
  });
}

/**
 * Test a set of tree rearrangements and make sure they cause the expected changes.
 */

var gDummySerial = 0;

function mutationTest(testSpec) {
  return async function() {
    const { walker } = await initInspectorFront(
      MAIN_DOMAIN + "inspector-traversal-data.html"
    );
    await loadSelectors(walker, testSpec.load || ["html"]);
    walker.autoCleanup = !!testSpec.autoCleanup;
    if (testSpec.preCheck) {
      testSpec.preCheck();
    }
    const onMutations = walker.once("mutations");

    await doMoves(testSpec.moves || []);

    // Some of these moves will trigger no mutation events,
    // so do a dummy change to the root node to trigger
    // a mutation event anyway.
    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [[gDummySerial++]],
      function(serial) {
        content.document.documentElement.setAttribute("data-dummy", serial);
      }
    );

    let mutations = await onMutations;

    // Filter out our dummy mutation.
    mutations = mutations.filter(change => {
      if (change.type == "attributes" && change.attributeName == "data-dummy") {
        return false;
      }
      return true;
    });
    await assertOwnershipTrees(walker);
    if (testSpec.postCheck) {
      testSpec.postCheck(walker, mutations);
    }
  };
}

// Verify that our dummy mutation works.
add_task(
  mutationTest({
    autoCleanup: false,
    postCheck(walker, mutations) {
      is(mutations.length, 0, "Dummy mutation is filtered out.");
    },
  })
);

// Test a simple move to a different location in the sibling list for the same
// parent.
add_task(
  mutationTest({
    autoCleanup: false,
    load: ["#longlist div"],
    moves: [["#a", "#longlist"]],
    postCheck(walker, mutations) {
      const remove = mutations[0];
      is(remove.type, "childList", "First mutation should be a childList.");
      ok(!!remove.removed.length, "First mutation should be a removal.");
      const add = mutations[1];
      is(
        add.type,
        "childList",
        "Second mutation should be a childList removal."
      );
      ok(!!add.added.length, "Second mutation should be an addition.");
      const a = add.added[0];
      is(a.id, "a", "Added node should be #a");
      is(a.parentNode(), remove.target, "Should still be a child of longlist.");
      is(
        remove.target,
        add.target,
        "First and second mutations should be against the same node."
      );
    },
  })
);

// Test a move to another location that is within our ownership tree.
add_task(
  mutationTest({
    autoCleanup: false,
    load: ["#longlist div", "#longlist-sibling"],
    moves: [["#a", "#longlist-sibling"]],
    postCheck(walker, mutations) {
      const remove = mutations[0];
      is(remove.type, "childList", "First mutation should be a childList.");
      ok(!!remove.removed.length, "First mutation should be a removal.");
      const add = mutations[1];
      is(
        add.type,
        "childList",
        "Second mutation should be a childList removal."
      );
      ok(!!add.added.length, "Second mutation should be an addition.");
      const a = add.added[0];
      is(a.id, "a", "Added node should be #a");
      is(a.parentNode(), add.target, "Should still be a child of longlist.");
      is(
        add.target.id,
        "longlist-sibling",
        "long-sibling should be the target."
      );
    },
  })
);

// Move an unseen node with a seen parent into our ownership tree - should generate a
// childList pair with no adds or removes.
add_task(
  mutationTest({
    autoCleanup: false,
    load: ["#longlist"],
    moves: [["#longlist-sibling", "#longlist"]],
    postCheck(walker, mutations) {
      is(mutations.length, 2, "Should generate two mutations");
      is(mutations[0].type, "childList", "Should be childList mutations.");
      is(mutations[0].added.length, 0, "Should have no adds.");
      is(mutations[0].removed.length, 0, "Should have no removes.");
      is(mutations[1].type, "childList", "Should be childList mutations.");
      is(mutations[1].added.length, 0, "Should have no adds.");
      is(mutations[1].removed.length, 0, "Should have no removes.");
    },
  })
);

// Move an unseen node with an unseen parent into our ownership tree.  Should only
// generate one childList mutation with no adds or removes.
add_task(
  mutationTest({
    autoCleanup: false,
    load: ["#longlist div"],
    moves: [["#longlist-sibling-firstchild", "#longlist"]],
    postCheck(walker, mutations) {
      is(mutations.length, 1, "Should generate two mutations");
      is(mutations[0].type, "childList", "Should be childList mutations.");
      is(mutations[0].added.length, 0, "Should have no adds.");
      is(mutations[0].removed.length, 0, "Should have no removes.");
    },
  })
);

// Move a node between unseen nodes, should generate no mutations.
add_task(
  mutationTest({
    autoCleanup: false,
    load: ["html"],
    moves: [["#longlist-sibling", "#longlist"]],
    postCheck(walker, mutations) {
      is(mutations.length, 0, "Should generate no mutations.");
    },
  })
);

// Orphan a node and don't clean it up
add_task(
  mutationTest({
    autoCleanup: false,
    load: ["#longlist div"],
    moves: [["#longlist", null]],
    postCheck(walker, mutations) {
      is(mutations.length, 1, "Should generate one mutation.");
      const change = mutations[0];
      is(change.type, "childList", "Should be a childList.");
      is(change.removed.length, 1, "Should have removed a child.");
      const ownership = clientOwnershipTree(walker);
      is(ownership.orphaned.length, 1, "Should have one orphaned subtree.");
      is(
        ownershipTreeSize(ownership.orphaned[0]),
        1 + 26 + 26,
        "Should have orphaned longlist, and 26 children, and 26 singleTextChilds"
      );
    },
  })
);

// Orphan a node, and do clean it up.
add_task(
  mutationTest({
    autoCleanup: true,
    load: ["#longlist div"],
    moves: [["#longlist", null]],
    postCheck(walker, mutations) {
      is(mutations.length, 1, "Should generate one mutation.");
      const change = mutations[0];
      is(change.type, "childList", "Should be a childList.");
      is(change.removed.length, 1, "Should have removed a child.");
      const ownership = clientOwnershipTree(walker);
      is(ownership.orphaned.length, 0, "Should have no orphaned subtrees.");
    },
  })
);

// Orphan a node by moving it into the tree but out of our visible subtree.
add_task(
  mutationTest({
    autoCleanup: false,
    load: ["#longlist div"],
    moves: [["#longlist", "#longlist-sibling"]],
    postCheck(walker, mutations) {
      is(mutations.length, 1, "Should generate one mutation.");
      const change = mutations[0];
      is(change.type, "childList", "Should be a childList.");
      is(change.removed.length, 1, "Should have removed a child.");
      const ownership = clientOwnershipTree(walker);
      is(ownership.orphaned.length, 1, "Should have one orphaned subtree.");
      is(
        ownershipTreeSize(ownership.orphaned[0]),
        1 + 26 + 26,
        "Should have orphaned longlist, 26 children, and 26 singleTextChilds."
      );
    },
  })
);

// Orphan a node by moving it into the tree but out of our visible subtree,
// and clean it up.
add_task(
  mutationTest({
    autoCleanup: true,
    load: ["#longlist div"],
    moves: [["#longlist", "#longlist-sibling"]],
    postCheck(walker, mutations) {
      is(mutations.length, 1, "Should generate one mutation.");
      const change = mutations[0];
      is(change.type, "childList", "Should be a childList.");
      is(change.removed.length, 1, "Should have removed a child.");
      const ownership = clientOwnershipTree(walker);
      is(ownership.orphaned.length, 0, "Should have no orphaned subtrees.");
    },
  })
);
