/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test shadow DOM content in the markupview.
// Note that many features are not yet enabled, but basic listing
// of elements should be working.
const TEST_URL = URL_ROOT + "doc_markup_anonymous.html";

add_task(async function() {
  Services.prefs.setBoolPref("dom.webcomponents.shadowdom.enabled", true);

  const {inspector} = await openInspectorForURL(TEST_URL);

  const shadowHostFront = await getNodeFront("#shadow", inspector.markup);
  is(shadowHostFront.numChildren, 3, "Children of the shadow host are correct");

  await inspector.markup.expandNode(shadowHostFront);
  await waitForMultipleChildrenUpdates(inspector);

  const shadowContainer = inspector.markup.getContainer(shadowHostFront);
  const containers = shadowContainer.getChildContainers();

  info("Checking the ::before pseudo element");
  const before = containers[1].node;
  await isEditingMenuDisabled(before, inspector);

  info("Checking shadow dom children");
  const shadowRootFront = containers[0].node;
  const children = await inspector.walker.children(shadowRootFront);

  is(shadowRootFront.numChildren, 2, "Children of the shadow root are counted");
  is(children.nodes.length, 2, "Children returned from walker");

  info("Checking the <h3> shadow element");
  const shadowChild1 = children.nodes[0];
  await isEditingMenuDisabled(shadowChild1, inspector);

  info("Checking the <select> shadow element");
  const shadowChild2 = children.nodes[1];
  await isEditingMenuDisabled(shadowChild2, inspector);
});
