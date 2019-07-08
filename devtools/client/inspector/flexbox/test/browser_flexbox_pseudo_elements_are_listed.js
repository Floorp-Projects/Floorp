/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pseudo-elements that are flex items do appear in the list of items.

const TEST_URI = URL_ROOT + "doc_flexbox_pseudos.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  // Select the flex container in the inspector.
  const onItemsListRendered = waitForDOM(
    doc,
    ".layout-flexbox-wrapper .flex-item-list"
  );
  await selectNode(".container", inspector);
  const [flexItemList] = await onItemsListRendered;

  const items = [...flexItemList.querySelectorAll("button .objectBox")];
  is(items.length, 2, "There are 2 items displayed in the list");

  is(items[0].textContent, "::before", "The first item is ::before");
  is(items[1].textContent, "::after", "The second item is ::after");
});
