/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Utility functions for collapsing markers into a waterfall.
 */

loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/performance/markers", true);

/**
 * Collapses markers into a tree-like structure.
 * @param object markerNode
 * @param array markersList
 * @param ?object blueprint
 */
function collapseMarkersIntoNode({ markerNode, markersList, blueprint }) {
  let { getCurrentParentNode, collapseMarker, addParentNode, popParentNode } = createParentNodeFactory(markerNode);
  blueprint = blueprint || TIMELINE_BLUEPRINT;

  for (let i = 0, len = markersList.length; i < len; i++) {
    let curr = markersList[i];

    let parentNode = getCurrentParentNode();
    let def = blueprint[curr.name];
    let collapse = def.collapseFunc || (() => null);
    let peek = distance => markersList[i + distance];
    let foundParent = false;

    let collapseInfo = collapse(parentNode, curr, peek);
    if (collapseInfo) {
      let { collapse, toParent, finalize } = collapseInfo;

      // If `toParent` is an object, use it as the next parent marker
      if (typeof toParent === "object") {
        addParentNode(toParent);
      }

      if (collapse) {
        collapseMarker(curr);
      }

      // If the marker specifies this parent marker is full,
      // pop it from the stack.
      if (finalize) {
        popParentNode();
      }
    } else {
      markerNode.submarkers.push(curr);
    }
  }
}

/**
 * Creates a parent marker, which functions like a regular marker,
 * but is able to hold additional child markers.
 *
 * The marker is seeded with values from `marker`.
 * @param object marker
 * @return object
 */
function makeParentMarkerNode (marker) {
  let node = Object.create(null);
  for (let prop in marker) {
    node[prop] = marker[prop];
  }
  node.submarkers = [];
  return node;
}

/**
 * Takes a root marker node and creates a hash of functions used
 * to manage the creation and nesting of additional parent markers.
 *
 * @param {object} root
 * @return {object}
 */
function createParentNodeFactory (root) {
  let parentMarkers = [];
  let factory = {
    /**
     * Pops the most recent parent node off the stack, finalizing it.
     * Sets the `end` time based on the most recent child if not defined.
     */
    popParentNode: () => {
      if (parentMarkers.length === 0) {
        throw new Error("Cannot pop parent markers when none exist.");
      }

      let lastParent = parentMarkers.pop();
      // If this finished parent marker doesn't have an end time,
      // so probably a synthesized marker, use the last marker's end time.
      if (lastParent.end == void 0) {
        lastParent.end = lastParent.submarkers[lastParent.submarkers.length - 1].end;
      }
      return lastParent;
    },

    /**
     * Returns the most recent parent node.
     */
    getCurrentParentNode: () => parentMarkers.length ? parentMarkers[parentMarkers.length - 1] : null,

    /**
     * Push a new parent node onto the stack and nest it with the
     * next most recent parent node, or root if no other parent nodes.
     */
    addParentNode: (marker) => {
      let parentMarker = makeParentMarkerNode(marker);
      (factory.getCurrentParentNode() || root).submarkers.push(parentMarker);
      parentMarkers.push(parentMarker);
    },

    /**
     * Push this marker into the most recent parent node.
     */
    collapseMarker: (marker) => {
      if (parentMarkers.length === 0) {
        throw new Error("Cannot collapse marker with no parents.");
      }
      factory.getCurrentParentNode().submarkers.push(marker);
    }
  };

  return factory;
}

exports.makeParentMarkerNode = makeParentMarkerNode;
exports.collapseMarkersIntoNode = collapseMarkersIntoNode;
