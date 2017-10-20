/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { flexboxSpec, gridSpec, layoutSpec } = require("devtools/shared/specs/layout");
const nodeFilterConstants = require("devtools/shared/dom-node-filter-constants");
const { getStringifiableFragments } =
  require("devtools/server/actors/utils/css-grid-utils");

loader.lazyRequireGetter(this, "CssLogic", "devtools/server/css-logic", true);

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
   *         The flexbox container element.
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

    let form = {
      actor: this.actorID,
      gridFragments: this.gridFragments,
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
   * Returns an array of FlexboxActor objects for all the flexbox containers found by
   * iterating below the given rootNode.
   *
   * @param  {Node|NodeActor} rootNode
   *         The root node to start iterating at.
   * @return {Array} An array of FlexboxActor objects.
   */
  getFlexbox(rootNode) {
    let flexboxes = [];

    if (!rootNode) {
      return flexboxes;
    }

    let treeWalker = this.walker.getDocumentWalker(rootNode,
      nodeFilterConstants.SHOW_ELEMENT);

    while (treeWalker.nextNode()) {
      let currentNode = treeWalker.currentNode;
      let computedStyle = CssLogic.getComputedStyle(currentNode);

      if (!computedStyle) {
        continue;
      }

      if (computedStyle.display == "inline-flex" || computedStyle.display == "flex") {
        let flexboxActor = new FlexboxActor(this, currentNode);
        flexboxes.push(flexboxActor);
      }
    }

    return flexboxes;
  },

  /**
   * Returns an array of FlexboxActor objects for all existing flexbox containers found by
   * iterating below the given rootNode and optionally including nested frames.
   *
   * @param  {NodeActor} rootNode
   * @param  {Boolean} traverseFrames
   *         Whether or not we should iterate through nested frames.
   * @return {Array} An array of FlexboxActor objects.
   */
  getAllFlexbox(rootNode, traverseFrames) {
    let flexboxes = [];

    if (!rootNode) {
      return flexboxes;
    }

    if (!traverseFrames) {
      return this.getFlexbox(rootNode.rawNode);
    }

    for (let {document} of this.tabActor.windows) {
      flexboxes = [...flexboxes, ...this.getFlexbox(document.documentElement)];
    }

    return flexboxes;
  },

  /**
   * Returns an array of GridActor objects for all the grid containers found by iterating
   * below the given rootNode.
   *
   * @param  {Node|NodeActor} rootNode
   *         The root node to start iterating at.
   * @return {Array} An array of GridActor objects.
   */
  getGrids(rootNode) {
    let grids = [];

    if (!rootNode) {
      return grids;
    }

    let treeWalker = this.walker.getDocumentWalker(rootNode,
      nodeFilterConstants.SHOW_ELEMENT);

    while (treeWalker.nextNode()) {
      let currentNode = treeWalker.currentNode;

      if (currentNode.getGridFragments && currentNode.getGridFragments().length > 0) {
        let gridActor = new GridActor(this, currentNode);
        grids.push(gridActor);
      }
    }

    return grids;
  },

  /**
   * Returns an array of GridActor objects for all existing grid containers found by
   * iterating below the given rootNode and optionally including nested frames.
   *
   * @param  {NodeActor} rootNode
   * @param  {Boolean} traverseFrames
   *         Whether or not we should iterate through nested frames.
   * @return {Array} An array of GridActor objects.
   */
  getAllGrids(rootNode, traverseFrames) {
    let grids = [];

    if (!rootNode) {
      return grids;
    }

    if (!traverseFrames) {
      return this.getGrids(rootNode.rawNode);
    }

    for (let {document} of this.tabActor.windows) {
      grids = [...grids, ...this.getGrids(document.documentElement)];
    }

    return grids;
  },
});

exports.FlexboxActor = FlexboxActor;
exports.GridActor = GridActor;
exports.LayoutActor = LayoutActor;
