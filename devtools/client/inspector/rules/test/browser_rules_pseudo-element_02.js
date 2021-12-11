/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pseudoelements are displayed correctly in the markup view.

const TEST_URI = URL_ROOT + "doc_pseudoelement.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector } = await openRuleView();

  const node = await getNodeFront("#topleft", inspector);
  const children = await inspector.markup.walker.children(node);

  is(children.nodes.length, 3, "Element has correct number of children");

  const beforeElement = children.nodes[0];
  is(
    beforeElement.tagName,
    "_moz_generated_content_before",
    "tag name is correct"
  );
  await selectNode(beforeElement, inspector);

  const afterElement = children.nodes[children.nodes.length - 1];
  is(
    afterElement.tagName,
    "_moz_generated_content_after",
    "tag name is correct"
  );
  await selectNode(afterElement, inspector);

  const listNode = await getNodeFront("#list", inspector);
  const listChildren = await inspector.markup.walker.children(listNode);

  is(listChildren.nodes.length, 4, "<li> has correct number of children");
  const markerElement = listChildren.nodes[0];
  is(
    markerElement.tagName,
    "_moz_generated_content_marker",
    "tag name is correct"
  );
  await selectNode(markerElement, inspector);

  const listBeforeElement = listChildren.nodes[1];
  is(
    listBeforeElement.tagName,
    "_moz_generated_content_before",
    "tag name is correct"
  );
  await selectNode(listBeforeElement, inspector);
});
