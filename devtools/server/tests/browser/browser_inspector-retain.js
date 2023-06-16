/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

// Retain a node, and a second-order child (in another document, for kicks)
// Release the parent of the top item, which should cause one retained orphan.

// Then unretain the top node, which should retain the orphan.

// Then change the source of the iframe, which should kill that orphan.

add_task(async function testRetain() {
  // The test does not make sense when EFT is enabled, as different documents will have
  // different walkers.
  if (isEveryFrameTargetEnabled()) {
    return;
  }

  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  // Get the toplevel body element and retain it.
  const bodyFront = await walker.querySelector(walker.rootNode, "body");
  await walker.retainNode(bodyFront);
  // Get an element in the child frame and retain it.
  const frame = await walker.querySelector(walker.rootNode, "#childFrame");
  const children = await walker.children(frame, { maxNodes: 1 });
  const childDoc = children.nodes[0];
  const childListFront = await walker.querySelector(childDoc, "#longlist");
  const originalOwnershipSize = await assertOwnershipTrees(walker);
  // and retain it.
  await walker.retainNode(childListFront);
  // OK, try releasing the parent of the first retained.
  await walker.releaseNode(bodyFront.parentNode());
  const clientTree = clientOwnershipTree(walker);

  // That request should have freed the parent of the first retained
  // but moved the rest into the retained orphaned tree.
  is(
    ownershipTreeSize(clientTree.root) +
      ownershipTreeSize(clientTree.retained[0]) +
      1,
    originalOwnershipSize,
    "Should have only lost one item overall."
  );
  is(walker._retainedOrphans.size, 1, "Should have retained one orphan");
  ok(
    walker._retainedOrphans.has(bodyFront),
    "Should have retained the expected node."
  );
  // Unretain the body, which should promote the childListFront to a retained orphan.
  await walker.unretainNode(bodyFront);
  await assertOwnershipTrees(walker);

  is(
    walker._retainedOrphans.size,
    1,
    "Should still only have one retained orphan."
  );
  ok(
    !walker._retainedOrphans.has(bodyFront),
    "Should have dropped the body node."
  );
  ok(
    walker._retainedOrphans.has(childListFront),
    "Should have retained the child node."
  );

  // Change the source of the iframe, which should kill the retained orphan.
  const onMutations = waitForMutation(walker, isUnretained);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.querySelector("#childFrame").src =
      "data:text/html,<html>new child</html>";
  });
  await onMutations;

  await assertOwnershipTrees(walker);
  is(walker._retainedOrphans.size, 0, "Should have no more retained orphans.");
});

// Get a hold of a node, remove it from the doc and retain it at the same time.
// We should always win that race (even though the mutation happens before the
// retain request), because we haven't issued `getMutations` yet.
add_task(async function testWinRace() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  const front = await walker.querySelector(walker.rootNode, "#a");
  const onMutation = waitForMutation(walker, isChildList);
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    const contentNode = content.document.querySelector("#a");
    contentNode.remove();
  });
  // Now wait for that mutation and retain response to come in.
  await walker.retainNode(front);
  await onMutation;

  await assertOwnershipTrees(walker);
  is(walker._retainedOrphans.size, 1, "Should have a retained orphan.");
  ok(
    walker._retainedOrphans.has(front),
    "Should have retained our expected node."
  );
  await walker.unretainNode(front);

  // Make sure we're clear for the next test.
  await assertOwnershipTrees(walker);
  is(walker._retainedOrphans.size, 0, "Should have no more retained orphans.");
});

// Same as above, but issue the request right after the 'new-mutations' event, so that
// we *lose* the race.
add_task(async function testLoseRace() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  const front = await walker.querySelector(walker.rootNode, "#z");
  const onMutation = walker.once("new-mutations");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    const contentNode = content.document.querySelector("#z");
    contentNode.remove();
  });
  await onMutation;

  // Verify that we have an outstanding request (no good way to tell that it's a
  // getMutations request, but there's nothing else it would be).
  is(walker._requests.length, 1, "Should have an outstanding request.");
  try {
    await walker.retainNode(front);
    ok(false, "Request should not have succeeded!");
  } catch (err) {
    // XXX: Switched to from ok() to todo_is() in Bug 1467712. Follow up in
    // 1500960
    // This is throwing because of
    // `gInspectee.querySelector("#z").parentNode = null;` two blocks above...
    // Even if you fix that, the test is still failing because "#a" was removed
    // by the previous test. I am switching this to "#z" because I think that
    // was the original intent. Still not failing with the expected error message
    // Needs more work.
    // ok(err, "noSuchActor", "Should have lost the race.");
    is(
      walker._retainedOrphans.size,
      0,
      "Should have no more retained orphans."
    );
    // Don't re-throw the error.
  }
});
