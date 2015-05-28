/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Utility functions for collapsing markers into a waterfall.
 */

loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/performance/global", true);

/**
 * Collapses markers into a tree-like structure. Currently, this only goes
 * one level deep.
 * @param object markerNode
 * @param array markersList
 */
function collapseMarkersIntoNode({ markerNode, markersList }) {
  let [getOrCreateParentNode, getCurrentParentNode, clearParentNode] = makeParentNodeFactory();

  for (let i = 0, len = markersList.length; i < len; i++) {
    let curr = markersList[i];
    let blueprint = TIMELINE_BLUEPRINT[curr.name];

    let parentNode = getCurrentParentNode();
    let collapse = blueprint.collapseFunc || (() => null);
    let peek = distance => markersList[i + distance];
    let collapseInfo = collapse(parentNode, curr, peek);

    if (collapseInfo) {
      let { toParent, withData, forceNew, forceEnd } = collapseInfo;

      // If the `forceNew` prop is set on the collapse info, then a new parent
      // marker needs to be created even if there is one already available.
      if (forceNew) {
        clearParentNode();
      }
      // If the `toParent` prop is set on the collapse info, then this marker
      // can be collapsed into a higher-level parent marker.
      if (toParent) {
        let parentNode = getOrCreateParentNode(markerNode, toParent, curr.start);
        parentNode.end = curr.end;
        parentNode.submarkers.push(curr);
        for (let key in withData) {
          parentNode[key] = withData[key];
        }
      }
      // If the `forceEnd` prop is set on the collapse info, then the higher-level
      // parent marker is full and should be finalized.
      if (forceEnd) {
        clearParentNode();
      }
    } else {
      clearParentNode();
      markerNode.submarkers.push(curr);
    }
  }
}

/**
 * Creates an empty parent marker, which functions like a regular marker,
 * but is able to hold additional child markers.
 * @param string name
 * @param number start [optional]
 * @param number end [optional]
 * @return object
 */
function makeEmptyMarkerNode(name, start, end) {
  return {
    name: name,
    start: start,
    end: end,
    submarkers: []
  };
}

/**
 * Creates a factory for markers containing other markers.
 * @return array[function]
 */
function makeParentNodeFactory() {
  let marker;

  return [
    /**
     * Gets the current parent marker for the given marker name. If it doesn't
     * exist, it creates it and appends it to another parent marker.
     * @param object owner
     * @param string name
     * @param number start
     * @return object
     */
    function getOrCreateParentNode(owner, name, start) {
      if (marker && marker.name == name) {
        return marker;
      } else {
        marker = makeEmptyMarkerNode(name, start);
        owner.submarkers.push(marker);
        return marker;
      }
    },

    /**
     * Gets the current marker marker.
     * @return object
     */
    function getCurrentParentNode() {
      return marker;
    },

    /**
     * Clears the current marker marker.
     */
    function clearParentNode() {
      marker = null;
    }
  ];
}

exports.makeEmptyMarkerNode = makeEmptyMarkerNode;
exports.collapseMarkersIntoNode = collapseMarkersIntoNode;
