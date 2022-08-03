/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Dummy container node used for the root document element.
 */
function RootContainer(markupView, node) {
  this.doc = markupView.doc;
  this.elt = this.doc.createElement("ul");
  // Root container has tree semantics for accessibility.
  this.elt.setAttribute("role", "tree");
  this.elt.setAttribute("tabindex", "0");
  this.elt.setAttribute("aria-dropeffect", "none");
  this.elt.container = this;
  this.children = this.elt;
  this.node = node;
  this.toString = () => "[root container]";
}

RootContainer.prototype = {
  hasChildren: true,
  expanded: true,
  update() {},
  destroy() {},

  /**
   * If the node has children, return the list of containers for all these children.
   * @return {Array} An array of child containers or null.
   */
  getChildContainers() {
    return [...this.children.children]
      .filter(node => node.container)
      .map(node => node.container);
  },

  /**
   * Set the expanded state of the container node.
   * @param  {Boolean} value
   */
  setExpanded() {},

  /**
   * Set an appropriate role of the container's children node.
   */
  setChildrenRole() {},

  /**
   * Set an appropriate DOM tree depth level for a node and its subtree.
   */
  updateLevel() {},

  isSlotted() {
    return false;
  },
};

module.exports = RootContainer;
