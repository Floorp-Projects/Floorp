/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the add node button and context menu items have the right state
// depending on the current selection.

const TEST_URL = URL_ROOT + "doc_inspector_add_node.html";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Select the DOCTYPE element");
  let {nodes} = await inspector.walker.children(inspector.walker.rootNode);
  await selectNode(nodes[0], inspector);
  assertState(false, inspector,
    "The button and item are disabled on DOCTYPE");

  info("Select the ::before pseudo-element");
  const body = await getNodeFront("body", inspector);
  ({nodes} = await inspector.walker.children(body));
  await selectNode(nodes[0], inspector);
  assertState(false, inspector,
    "The button and item are disabled on a pseudo-element");

  info("Select the svg element");
  await selectNode("svg", inspector);
  assertState(false, inspector,
    "The button and item are disabled on a SVG element");

  info("Select the div#foo element");
  await selectNode("#foo", inspector);
  assertState(true, inspector,
    "The button and item are enabled on a DIV element");

  info("Select the documentElement element (html)");
  await selectNode("html", inspector);
  assertState(false, inspector,
    "The button and item are disabled on the documentElement");

  info("Select the iframe element");
  await selectNode("iframe", inspector);
  assertState(false, inspector,
    "The button and item are disabled on an IFRAME element");
});

function assertState(isEnabled, inspector, desc) {
  const doc = inspector.panelDoc;
  const btn = doc.querySelector("#inspector-element-add-button");

  // Force an update of the context menu to make sure menu items are updated
  // according to the current selection. This normally happens when the menu is
  // opened, but for the sake of this test's simplicity, we directly call the
  // private update function instead.
  const allMenuItems = openContextMenuAndGetAllItems(inspector);
  const menuItem = allMenuItems.find(item => item.id === "node-menu-add");
  ok(menuItem, "The item is in the menu");
  is(!menuItem.disabled, isEnabled, desc);

  is(!btn.hasAttribute("disabled"), isEnabled, desc);
}
