/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the add node button and context menu items have the right state
// depending on the current selection.

const TEST_URL = URL_ROOT + "doc_inspector_add_node.html";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  info("Select the DOCTYPE element");
  let {nodes} = yield inspector.walker.children(inspector.walker.rootNode);
  yield selectNode(nodes[0], inspector);
  assertState(false, inspector,
    "The button and item are disabled on DOCTYPE");

  info("Select the ::before pseudo-element");
  let body = yield getNodeFront("body", inspector);
  ({nodes} = yield inspector.walker.children(body));
  yield selectNode(nodes[0], inspector);
  assertState(false, inspector,
    "The button and item are disabled on a pseudo-element");

  info("Select the svg element");
  yield selectNode("svg", inspector);
  assertState(false, inspector,
    "The button and item are disabled on a SVG element");

  info("Select the div#foo element");
  yield selectNode("#foo", inspector);
  assertState(true, inspector,
    "The button and item are enabled on a DIV element");

  info("Select the documentElement element (html)");
  yield selectNode("html", inspector);
  assertState(false, inspector,
    "The button and item are disabled on the documentElement");

  info("Select the iframe element");
  yield selectNode("iframe", inspector);
  assertState(false, inspector,
    "The button and item are disabled on an IFRAME element");
});

function assertState(isEnabled, inspector, desc) {
  let doc = inspector.panelDoc;
  let btn = doc.querySelector("#inspector-element-add-button");
  let item = doc.querySelector("#node-menu-add");

  // Force an update of the context menu to make sure menu items are updated
  // according to the current selection. This normally happens when the menu is
  // opened, but for the sake of this test's simplicity, we directly call the
  // private update function instead.
  inspector._setupNodeMenu({target: {}});

  is(!btn.hasAttribute("disabled"), isEnabled, desc);
  is(!item.hasAttribute("disabled"), isEnabled, desc);
}
