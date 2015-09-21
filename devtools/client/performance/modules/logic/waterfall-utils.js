/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Utility functions for collapsing markers into a waterfall.
 */

loader.lazyRequireGetter(this, "extend",
  "sdk/util/object", true);
loader.lazyRequireGetter(this, "MarkerUtils",
  "devtools/client/performance/modules/logic/marker-utils");

/**
 * Creates a parent marker, which functions like a regular marker,
 * but is able to hold additional child markers.
 *
 * The marker is seeded with values from `marker`.
 * @param object marker
 * @return object
 */
function createParentNode (marker) {
  return extend(marker, { submarkers: [] });
}


/**
 * Collapses markers into a tree-like structure.
 * @param object rootNode
 * @param array markersList
 * @param array filter
 */
function collapseMarkersIntoNode({ rootNode, markersList, filter }) {
  let { getCurrentParentNode, pushNode, popParentNode } = createParentNodeFactory(rootNode);

  for (let i = 0, len = markersList.length; i < len; i++) {
    let curr = markersList[i];

    // If this marker type should not be displayed, just skip
    if (!MarkerUtils.isMarkerValid(curr, filter)) {
      continue;
    }

    let parentNode = getCurrentParentNode();
    let blueprint = MarkerUtils.getBlueprintFor(curr);

    let nestable = "nestable" in blueprint ? blueprint.nestable : true;
    let collapsible = "collapsible" in blueprint ? blueprint.collapsible : true;

    let finalized = null;

    // If this marker is collapsible, turn it into a parent marker.
    // If there are no children within it later, it will be turned
    // back into a normal node.
    if (collapsible) {
      curr = createParentNode(curr);
    }

    // If not nestible, just push it inside the root node,
    // like console.time/timeEnd.
    if (!nestable) {
      pushNode(rootNode, curr);
      continue;
    }

    // First off, if any parent nodes exist, finish them off
    // recursively upwards if this marker is outside their ranges and nestable.
    while (!finalized && parentNode) {
      // If this marker is eclipsed by the current parent marker,
      // make it a child of the current parent and stop
      // going upwards.
      if (nestable && curr.end <= parentNode.end) {
        pushNode(parentNode, curr);
        finalized = true;
        break;
      }

      // If this marker is still nestable, but outside of the range
      // of the current parent, iterate upwards on the next parent
      // and finalize the current parent.
      if (nestable) {
        popParentNode();
        parentNode = getCurrentParentNode();
        continue;
      }
    }

    if (!finalized) {
      pushNode(rootNode, curr);
    }
  }
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

      // If no children were ever pushed into this parent node,
      // remove it's submarkers so it behaves like a non collapsible
      // node.
      if (!lastParent.submarkers.length) {
        delete lastParent.submarkers;
      }

      return lastParent;
    },

    /**
     * Returns the most recent parent node.
     */
    getCurrentParentNode: () => parentMarkers.length ? parentMarkers[parentMarkers.length - 1] : null,

    /**
     * Push this marker into the most recent parent node.
     */
    pushNode: (parent, marker) => {
      parent.submarkers.push(marker);

      // If pushing a parent marker, track it as the top of
      // the parent stack.
      if (marker.submarkers) {
        parentMarkers.push(marker);
      }
    }
  };

  return factory;
}

exports.createParentNode = createParentNode;
exports.collapseMarkersIntoNode = collapseMarkersIntoNode;
