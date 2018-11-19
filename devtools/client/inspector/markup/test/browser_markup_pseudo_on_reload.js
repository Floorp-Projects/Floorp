/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that reloading the page when an element with sibling pseudo elements is selected
// does not result in missing elements in the markup-view after reload.
// Non-regression test for bug 1506792.

const TEST_URL = URL_ROOT + "doc_markup_pseudo.html";

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);

  await selectNode("div", inspector);

  info("Check that the markup-view shows the expected nodes before reload");
  await checkMarkupView(inspector);

  await reloadPage(inspector, testActor);

  info("Check that the markup-view shows the expected nodes after reload");
  await checkMarkupView(inspector);
});

async function checkMarkupView(inspector) {
  const articleContainer = await getContainerForSelector("article", inspector);
  ok(articleContainer, "The parent <article> element was found");

  const childrenContainers = articleContainer.getChildContainers();
  const beforeNode = childrenContainers[0].node;
  const divNode = childrenContainers[1].node;
  const afterNode = childrenContainers[2].node;

  ok(beforeNode.isBeforePseudoElement, "The first child is the ::before pseudo element");
  is(divNode.displayName, "div", "The second child is the <div> element");
  ok(afterNode.isAfterPseudoElement, "The last child is the ::after pseudo element");
}
