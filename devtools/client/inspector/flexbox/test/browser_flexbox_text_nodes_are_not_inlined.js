/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that single child text nodes that are also flex items can be selected in the
// flexbox inspector.
// This means that they are not inlined like normal single child text nodes, since
// selecting them in the flexbox inspector also means selecting them in the markup view.

const TEST_URI = URL_ROOT + "doc_flexbox_text_nodes.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  // Select the flex container in the inspector.
  const onItemsListRendered = waitForDOM(doc, ".layout-flexbox-wrapper .flex-item-list");
  await selectNode(".container.single-child", inspector);
  const [flexItemList] = await onItemsListRendered;

  const items = [...flexItemList.querySelectorAll("button .objectBox")];
  is(items.length, 1, "There is 1 item displayed in the list");
  is(items[0].textContent, "#text", "The item in the list is a text node");

  info("Click on the item to select it");
  const onFlexItemOutlineRendered = waitForDOM(doc, ".flex-outline-container");
  items[0].closest("button").click();
  const [flexOutlineContainer] = await onFlexItemOutlineRendered;
  ok(flexOutlineContainer,
     "The flex outline is displayed for a single child short text node too");

  ok(inspector.selection.isTextNode(),
     "The current inspector selection is the text node");

  const markupContainer = inspector.markup.getContainer(inspector.selection.nodeFront);
  is(markupContainer.elt.textContent, "short text", "This is the right text node");
});
