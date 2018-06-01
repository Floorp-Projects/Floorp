/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Utility functions for collapsing markers into a waterfall.
 */

const { MarkerBlueprintUtils } = require("devtools/client/performance/modules/marker-blueprint-utils");

/**
 * Creates a parent marker, which functions like a regular marker,
 * but is able to hold additional child markers.
 *
 * The marker is seeded with values from `marker`.
 * @param object marker
 * @return object
 */
function createParentNode(marker) {
  return Object.assign({}, marker, { submarkers: [] });
}

/**
 * Collapses markers into a tree-like structure.
 * @param object rootNode
 * @param array markersList
 * @param array filter
 */
function collapseMarkersIntoNode({ rootNode, markersList, filter }) {
  const {
    getCurrentParentNode,
    pushNode,
    popParentNode
  } = createParentNodeFactory(rootNode);

  for (let i = 0, len = markersList.length; i < len; i++) {
    const curr = markersList[i];

    // If this marker type should not be displayed, just skip
    if (!MarkerBlueprintUtils.shouldDisplayMarker(curr, filter)) {
      continue;
    }

    let parentNode = getCurrentParentNode();
    const blueprint = MarkerBlueprintUtils.getBlueprintFor(curr);

    const nestable = "nestable" in blueprint ? blueprint.nestable : true;
    const collapsible = "collapsible" in blueprint ? blueprint.collapsible : true;

    let finalized = false;

    // Extend the marker with extra properties needed in the marker tree
    const extendedProps = { index: i };
    if (collapsible) {
      extendedProps.submarkers = [];
    }
    Object.assign(curr, extendedProps);

    // If not nestible, just push it inside the root node. Additionally,
    // markers originating outside the main thread are considered to be
    // "never collapsible", to avoid confusion.
    // A beter solution would be to collapse every marker with its siblings
    // from the same thread, but that would require a thread id attached
    // to all markers, which is potentially expensive and rather useless at
    // the moment, since we don't really have that many OTMT markers.
    if (!nestable || curr.isOffMainThread) {
      pushNode(rootNode, curr);
      continue;
    }

    // First off, if any parent nodes exist, finish them off
    // recursively upwards if this marker is outside their ranges and nestable.
    while (!finalized && parentNode) {
      // If this marker is eclipsed by the current parent marker,
      // make it a child of the current parent and stop going upwards.
      // If the markers aren't from the same process, attach them to the root
      // node as well. Every process has its own main thread.
      if (nestable &&
          curr.start >= parentNode.start &&
          curr.end <= parentNode.end &&
          curr.processType == parentNode.processType) {
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
function createParentNodeFactory(root) {
  const parentMarkers = [];
  const factory = {
    /**
     * Pops the most recent parent node off the stack, finalizing it.
     * Sets the `end` time based on the most recent child if not defined.
     */
    popParentNode: () => {
      if (parentMarkers.length === 0) {
        throw new Error("Cannot pop parent markers when none exist.");
      }

      const lastParent = parentMarkers.pop();

      // If this finished parent marker doesn't have an end time,
      // so probably a synthesized marker, use the last marker's end time.
      if (lastParent.end == void 0) {
        lastParent.end = lastParent.submarkers[lastParent.submarkers.length - 1].end;
      }

      // If no children were ever pushed into this parent node,
      // remove its submarkers so it behaves like a non collapsible
      // node.
      if (!lastParent.submarkers.length) {
        delete lastParent.submarkers;
      }

      return lastParent;
    },

    /**
     * Returns the most recent parent node.
     */
    getCurrentParentNode: () => parentMarkers.length
      ? parentMarkers[parentMarkers.length - 1]
      : null,

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
