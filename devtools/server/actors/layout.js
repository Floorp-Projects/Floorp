/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { getStringifiableFragments } =
  require("devtools/server/actors/utils/css-grid-utils");
const { gridSpec, layoutSpec } = require("devtools/shared/specs/layout");

/**
 * Set of actors the expose the CSS layout information to the devtools protocol clients.
 *
 * The |Layout| actor is the main entry point. It is used to get various CSS
 * layout-related information from the document.
 *
 * The |Grid| actor provides the grid fragment information to inspect the grid container.
 */

/**
 * The GridActor provides information about a given grid's fragment data.
 */
var GridActor = ActorClassWithSpec(gridSpec, {
  /**
   * @param  {LayoutActor} layoutActor
   *         The LayoutActor instance.
   * @param  {DOMNode} containerEl
   *         The grid container element.
   */
  initialize: function (layoutActor, containerEl) {
    Actor.prototype.initialize.call(this, layoutActor.conn);

    this.containerEl = containerEl;
    this.walker = layoutActor.walker;
  },

  destroy: function () {
    Actor.prototype.destroy.call(this);

    this.containerEl = null;
    this.gridFragments = null;
    this.walker = null;
  },

  form: function (detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    // Seralize the grid fragment data into JSON so protocol.js knows how to write
    // and read the data.
    let gridFragments = this.containerEl.getGridFragments();
    this.gridFragments = getStringifiableFragments(gridFragments);

    let form = {
      actor: this.actorID,
      gridFragments: this.gridFragments
    };

    return form;
  },
});

/**
 * The CSS layout actor provides layout information for the given document.
 */
var LayoutActor = ActorClassWithSpec(layoutSpec, {
  initialize: function (conn, tabActor, walker) {
    Actor.prototype.initialize.call(this, conn);

    this.tabActor = tabActor;
    this.walker = walker;
  },

  destroy: function () {
    Actor.prototype.destroy.call(this);

    this.tabActor = null;
    this.walker = null;
  },

  /**
   * Returns an array of GridActor objects for all the grid containers found by iterating
   * below the given rootNode.
   *
   * @param  {Node|NodeActor} rootNode
   *         The root node to start iterating at.
   * @return {Array} An array of GridActor objects.
   */
  getGrids: function (rootNode) {
    let grids = [];

    if (!rootNode) {
      return grids;
    }

    let treeWalker = this.walker.getDocumentWalker(rootNode);
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
  getAllGrids: function (rootNode, traverseFrames) {
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

exports.GridActor = GridActor;
exports.LayoutActor = LayoutActor;
