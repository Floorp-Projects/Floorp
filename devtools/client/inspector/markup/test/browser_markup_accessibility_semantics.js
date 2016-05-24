/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that inspector markup view has all expected ARIA properties set and
// updated.

const TOP_CONTAINER_LEVEL = 3;

add_task(function* () {
  let {inspector} = yield openInspectorForURL(`
    data:text/html;charset=utf-8,
    <h1>foo</h1>
    <span>bar</span>
    <ul>
      <li></li>
    </ul>`);
  let markup = inspector.markup;
  let doc = markup.doc;
  let win = doc.defaultView;

  let rootElt = markup.getContainer(markup._rootNode).elt;
  let bodyContainer = yield getContainerForSelector("body", inspector);
  let spanContainer = yield getContainerForSelector("span", inspector);
  let headerContainer = yield getContainerForSelector("h1", inspector);
  let listContainer = yield getContainerForSelector("ul", inspector);

  // Focus on the tree element.
  rootElt.focus();

  // Test tree related semantics
  is(rootElt.getAttribute("role"), "tree",
    "Root container should have tree semantics");
  is(rootElt.getAttribute("aria-dropeffect"), "none",
    "By default root container's drop effect should be set to none");
  is(rootElt.getAttribute("aria-activedescendant"),
    bodyContainer.tagLine.getAttribute("id"),
    "Default active descendant should be set to body");
  is(bodyContainer.tagLine.getAttribute("aria-level"), TOP_CONTAINER_LEVEL - 1,
    "Body container tagLine should have nested level up to date");
  [spanContainer, headerContainer, listContainer].forEach(container => {
    let treeitem = container.tagLine;
    is(treeitem.getAttribute("role"), "treeitem",
      "Child container tagLine elements should have tree item semantics");
    is(treeitem.getAttribute("aria-level"), TOP_CONTAINER_LEVEL,
      "Child container tagLine should have nested level up to date");
    is(treeitem.getAttribute("aria-grabbed"), "false",
      "Child container should be draggable but not grabbed by default");
    is(container.children.getAttribute("role"), "group",
      "Container with children should have its children element have group " +
      "semantics");
    ok(treeitem.id, "Tree item should have id assigned");
    if (container.closeTagLine) {
      is(container.closeTagLine.getAttribute("role"), "presentation",
        "Ignore closing tag");
    }
    if (container.expander) {
      is(container.expander.getAttribute("role"), "presentation",
        "Ignore expander");
    }
  });

  // Test expanding/expandable semantics
  ok(!spanContainer.tagLine.hasAttribute("aria-expanded"),
    "Non expandable tree items should not have aria-expanded attribute");
  ok(!headerContainer.tagLine.hasAttribute("aria-expanded"),
    "Non expandable tree items should not have aria-expanded attribute");
  is(listContainer.tagLine.getAttribute("aria-expanded"), "false",
    "Closed tree item should have aria-expanded unset");

  info("Selecting and expanding list container");
  let updated = waitForMultipleChildrenUpdates(inspector);
  yield selectNode("ul", inspector);
  EventUtils.synthesizeKey("VK_RIGHT", {}, win);
  yield updated;

  is(rootElt.getAttribute("aria-activedescendant"),
    listContainer.tagLine.getAttribute("id"),
    "Active descendant should not be set to list container tagLine");
  is(listContainer.tagLine.getAttribute("aria-expanded"), "true",
    "Open tree item should have aria-expanded set");
  let listItemContainer = yield getContainerForSelector("li", inspector);
  is(listItemContainer.tagLine.getAttribute("aria-level"),
    TOP_CONTAINER_LEVEL + 1,
    "Grand child container tagLine should have nested level up to date");
  is(listItemContainer.children.getAttribute("role"), "presentation",
    "Container with no children should have its children element ignored by " +
    "accessibility");

  info("Collapsing list container");
  updated = waitForMultipleChildrenUpdates(inspector);
  EventUtils.synthesizeKey("VK_LEFT", {}, win);
  yield updated;

  is(listContainer.tagLine.getAttribute("aria-expanded"), "false",
    "Closed tree item should have aria-expanded unset");
});

