/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { flexboxSpec, gridSpec, layoutSpec } = require("devtools/shared/specs/layout");
const nodeFilterConstants = require("devtools/shared/dom-node-filter-constants");
const { getStringifiableFragments } =
  require("devtools/server/actors/utils/css-grid-utils");

loader.lazyRequireGetter(this, "nodeConstants", "devtools/shared/dom-node-constants");
loader.lazyRequireGetter(this, "CssLogic", "devtools/server/actors/inspector/css-logic", true);

/**
 * Set of actors the expose the CSS layout information to the devtools protocol clients.
 *
 * The |Layout| actor is the main entry point. It is used to get various CSS
 * layout-related information from the document.
 *
 * The |Flexbox| actor provides the container node information to inspect the flexbox
 * container.
 *
 * The |Grid| actor provides the grid fragment information to inspect the grid container.
 */

const FlexboxActor = ActorClassWithSpec(flexboxSpec, {
  /**
   * @param  {LayoutActor} layoutActor
   *         The LayoutActor instance.
   * @param  {DOMNode} containerEl
   *         The flex container element.
   */
  initialize(layoutActor, containerEl) {
    Actor.prototype.initialize.call(this, layoutActor.conn);

    this.containerEl = containerEl;
    this.walker = layoutActor.walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.containerEl = null;
    this.walker = null;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let form = {
      actor: this.actorID,
    };

    // If the WalkerActor already knows the container element, then also return its
    // ActorID so we avoid the client from doing another round trip to get it in many
    // cases.
    if (this.walker.hasNode(this.containerEl)) {
      form.containerNodeActorID = this.walker.getNode(this.containerEl).actorID;
    }

    return form;
  },
});

/**
 * The GridActor provides information about a given grid's fragment data.
 */
const GridActor = ActorClassWithSpec(gridSpec, {
  /**
   * @param  {LayoutActor} layoutActor
   *         The LayoutActor instance.
   * @param  {DOMNode} containerEl
   *         The grid container element.
   */
  initialize(layoutActor, containerEl) {
    Actor.prototype.initialize.call(this, layoutActor.conn);

    this.containerEl = containerEl;
    this.walker = layoutActor.walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.containerEl = null;
    this.gridFragments = null;
    this.walker = null;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    // Seralize the grid fragment data into JSON so protocol.js knows how to write
    // and read the data.
    let gridFragments = this.containerEl.getGridFragments();
    this.gridFragments = getStringifiableFragments(gridFragments);

    // Record writing mode and text direction for use by the grid outline.
    let { direction, writingMode } = CssLogic.getComputedStyle(this.containerEl);

    let form = {
      actor: this.actorID,
      direction,
      gridFragments: this.gridFragments,
      writingMode,
    };

    // If the WalkerActor already knows the container element, then also return its
    // ActorID so we avoid the client from doing another round trip to get it in many
    // cases.
    if (this.walker.hasNode(this.containerEl)) {
      form.containerNodeActorID = this.walker.getNode(this.containerEl).actorID;
    }

    return form;
  },
});

/**
 * The CSS layout actor provides layout information for the given document.
 */
const LayoutActor = ActorClassWithSpec(layoutSpec, {
  initialize(conn, tabActor, walker) {
    Actor.prototype.initialize.call(this, conn);

    this.tabActor = tabActor;
    this.walker = walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.tabActor = null;
    this.walker = null;
  },

  /**
   * Returns the flex container found by iterating on the given selected node. The current
   * node can be a flex container or flex item. If it is a flex item, returns the parent
   * flex container. Otherwise, return null if the current or parent node is not a flex
   * container.
   *
   * @param  {Node|NodeActor} node
   *         The node to start iterating at.
   * @return {FlexboxActor|Null} The FlexboxActor of the flex container of the give node.
   * Otherwise, returns null.
   */
  getCurrentFlexbox(node) {
    if (isNodeDead(node)) {
      return null;
    }

    // Given node can either be a Node or a NodeActor.
    if (node.rawNode) {
      node = node.rawNode;
    }

    let treeWalker = this.walker.getDocumentWalker(node,
      nodeFilterConstants.SHOW_ELEMENT);
    let currentNode = treeWalker.currentNode;
    let displayType = this.walker.getNode(currentNode).displayType;

    if (!displayType) {
      return null;
    }

    // Check if the current node is a flex container.
    if (displayType == "inline-flex" || displayType == "flex") {
      return new FlexboxActor(this, treeWalker.currentNode);
    }

    // Otherwise, check if this is a flex item or the parent node is a flex container.
    while ((currentNode = treeWalker.parentNode())) {
      if (!currentNode) {
        break;
      }

      displayType = this.walker.getNode(currentNode).displayType;

      switch (displayType) {
        case "inline-flex":
        case "flex":
          return new FlexboxActor(this, currentNode);
        case "contents":
          // Continue walking up the tree since the parent node is a content element.
          continue;
      }

      break;
    }

    return null;
  },

  /**
   * Returns an array of GridActor objects for all the grid elements contained in the
   * given root node.
   *
   * @param  {Node|NodeActor} node
   *         The root node for grid elements
   * @return {Array} An array of GridActor objects.
   */
  getGrids(node) {
    if (isNodeDead(node)) {
      return [];
    }

    // Root node can either be a Node or a NodeActor.
    if (node.rawNode) {
      node = node.rawNode;
    }

    // Root node can be a #document object, which does not support getElementsWithGrid.
    if (node.nodeType === nodeConstants.DOCUMENT_NODE) {
      node = node.documentElement;
    }

    let gridElements = node.getElementsWithGrid();
    let gridActors = gridElements.map(n => new GridActor(this, n));

    let frames = node.querySelectorAll("iframe, frame");
    for (let frame of frames) {
      gridActors = gridActors.concat(this.getGrids(frame.contentDocument));
    }

    return gridActors;
  },
});

function isNodeDead(node) {
  return !node || (node.rawNode && Cu.isDeadWrapper(node.rawNode));
}

exports.FlexboxActor = FlexboxActor;
exports.GridActor = GridActor;
exports.LayoutActor = LayoutActor;
