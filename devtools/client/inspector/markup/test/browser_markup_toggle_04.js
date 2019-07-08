/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test expanding elements by clicking on expand badge.

const TEST_URL = URL_ROOT + "doc_markup_toggle.html";

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);

  info("Getting the container for the UL parent element");
  const container = await getContainerForSelector("ul", inspector);
  ok(!container.mustExpand, "UL element !mustExpand");
  ok(container.canExpand, "UL element canExpand");

  info("Clicking on the UL parent expand badge, and waiting for children");
  const onChildren = waitForChildrenUpdated(inspector);
  const onUpdated = inspector.once("inspector-updated");

  EventUtils.synthesizeMouseAtCenter(
    container.editor.expandBadge,
    {},
    inspector.markup.doc.defaultView
  );
  await onChildren;
  await onUpdated;

  info("Checking that child LI elements have been created");
  const numLi = await testActor.getNumberOfElementMatches("li");
  for (let i = 0; i < numLi; i++) {
    const liContainer = await getContainerForSelector(
      `li:nth-child(${i + 1})`,
      inspector
    );
    ok(liContainer, "A container for the child LI element was created");
  }
  ok(container.expanded, "Parent UL container is expanded");
});
