/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from inspector-helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

async function loadChildSelector(walker, selector) {
  const frame = await walker.querySelector(walker.rootNode, "#childFrame");
  ok(
    frame.numChildren > 0,
    "Child frame should consider its loaded document as a child."
  );
  const children = await walker.children(frame);
  const nodeList = await walker.querySelectorAll(children.nodes[0], selector);
  return nodeList.items();
}

function getUnloadedDoc(mutations) {
  for (const change of mutations) {
    if (isUnload(change)) {
      return change.target;
    }
  }
  return null;
}

add_task(async function loadNewChild() {
  const { target, walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  // Load a bunch of fronts for actors inside the child frame.
  await loadChildSelector(walker, "#longlist div");
  const onMutations = waitForMutation(walker, isChildList);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const childFrame = content.document.querySelector("#childFrame");
    childFrame.src = "data:text/html,<html>new child</html>";
  });
  let mutations = await onMutations;
  const unloaded = getUnloadedDoc(mutations);
  mutations = assertSrcChange(mutations);
  mutations = assertUnload(mutations);
  mutations = assertFrameLoad(mutations);
  mutations = assertChildList(mutations);

  is(mutations.length, 0, "Got the expected mutations.");

  assertOwnershipTrees(walker);

  return checkMissing(target, unloaded);
});

add_task(async function loadNewChild() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  // Load a bunch of fronts for actors inside the child frame.
  await loadChildSelector(walker, "#longlist div");
  let onMutations = waitForMutation(walker, isChildList);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const childFrame = content.document.querySelector("#childFrame");
    childFrame.src = "data:text/html,<html>new child</html>";
  });
  await onMutations;

  onMutations = waitForMutation(walker, isChildList);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    // The first load went through as expected (as tested in loadNewChild)
    // Now change the source again, but this time we *don't* expect
    // an unload, because we haven't seen the new child document yet.
    const childFrame = content.document.querySelector("#childFrame");
    childFrame.src = "data:text/html,<html>second new child</html>";
  });
  let mutations = await onMutations;
  mutations = assertSrcChange(mutations);
  mutations = assertFrameLoad(mutations);
  mutations = assertChildList(mutations);
  ok(!getUnloadedDoc(mutations), "Should not have gotten an unload.");

  is(mutations.length, 0, "Got the expected mutations.");

  assertOwnershipTrees(walker);
});

add_task(async function loadNewChildTwiceAndCareAboutIt() {
  const { target, walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  // Load a bunch of fronts for actors inside the child frame.
  await loadChildSelector(walker, "#longlist div");
  let onMutations = waitForMutation(walker, isChildList);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const childFrame = content.document.querySelector("#childFrame");
    childFrame.src = "data:text/html,<html>new child</html>";
  });
  await onMutations;
  // Read the new child
  await loadChildSelector(walker, "#longlist div");

  onMutations = waitForMutation(walker, isChildList);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    // Now change the source again, and expect the same results as loadNewChild.
    const childFrame = content.document.querySelector("#childFrame");
    childFrame.src = "data:text/html,<html>second new child</html>";
  });
  let mutations = await onMutations;
  const unloaded = getUnloadedDoc(mutations);

  mutations = assertSrcChange(mutations);
  mutations = assertUnload(mutations);
  mutations = assertFrameLoad(mutations);
  mutations = assertChildList(mutations);

  is(mutations.length, 0, "Got the expected mutations.");

  assertOwnershipTrees(walker);

  return checkMissing(target, unloaded);
});

add_task(async function testBack() {
  const { target, walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );
  // Load a bunch of fronts for actors inside the child frame.
  await loadChildSelector(walker, "#longlist div");
  let onMutations = waitForMutation(walker, isChildList);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const childFrame = content.document.querySelector("#childFrame");
    childFrame.src = "data:text/html,<html>new child</html>";
  });
  await onMutations;

  // Read the new child
  await loadChildSelector(walker, "#longlist div");

  onMutations = waitForMutation(walker, isChildList);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    // Now use history.back to change the source,
    // and expect the same results as loadNewChild.
    const childFrame = content.document.querySelector("#childFrame");
    childFrame.contentWindow.history.back();
  });
  let mutations = await onMutations;
  const unloaded = getUnloadedDoc(mutations);
  mutations = assertSrcChange(mutations);
  mutations = assertUnload(mutations);
  mutations = assertFrameLoad(mutations);
  mutations = assertChildList(mutations);
  is(mutations.length, 0, "Got the expected mutations.");

  assertOwnershipTrees(walker);

  return checkMissing(target, unloaded);
});
