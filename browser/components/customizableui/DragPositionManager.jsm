/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var gManagers = new WeakMap();

const kPaletteId = "customization-palette";

var EXPORTED_SYMBOLS = ["DragPositionManager"];

function AreaPositionManager(aContainer) {
  // Caching the direction and bounds of the container for quick access later:
  let window = aContainer.ownerGlobal;
  this._dir = window.getComputedStyle(aContainer).direction;
  let containerRect = aContainer.getBoundingClientRect();
  this._containerInfo = {
    left: containerRect.left,
    right: containerRect.right,
    top: containerRect.top,
    width: containerRect.width
  };
  this._horizontalDistance = null;
  this.update(aContainer);
}

AreaPositionManager.prototype = {
  _nodePositionStore: null,

  update(aContainer) {
    this._nodePositionStore = new WeakMap();
    let last = null;
    let singleItemHeight;
    for (let child of aContainer.children) {
      if (child.hidden) {
        continue;
      }
      let coordinates = this._lazyStoreGet(child);
      // We keep a baseline horizontal distance between nodes around
      // for use when we can't compare with previous/next nodes
      if (!this._horizontalDistance && last) {
        this._horizontalDistance = coordinates.left - last.left;
      }
      // We also keep the basic height of items for use below:
      if (!singleItemHeight) {
        singleItemHeight = coordinates.height;
      }
      last = coordinates;
    }
    this._heightToWidthFactor = this._containerInfo.width / singleItemHeight;
  },

  /**
   * Find the closest node in the container given the coordinates.
   * "Closest" is defined in a somewhat strange manner: we prefer nodes
   * which are in the same row over nodes that are in a different row.
   * In order to implement this, we use a weighted cartesian distance
   * where dy is more heavily weighted by a factor corresponding to the
   * ratio between the container's width and the height of its elements.
   */
  find(aContainer, aX, aY) {
    let closest = null;
    let minCartesian = Number.MAX_VALUE;
    let containerX = this._containerInfo.left;
    let containerY = this._containerInfo.top;
    for (let node of aContainer.children) {
      let coordinates = this._lazyStoreGet(node);
      let offsetX = coordinates.x - containerX;
      let offsetY = coordinates.y - containerY;
      let hDiff = offsetX - aX;
      let vDiff = offsetY - aY;
      // Then compensate for the height/width ratio so that we prefer items
      // which are in the same row:
      hDiff /= this._heightToWidthFactor;

      let cartesianDiff = hDiff * hDiff + vDiff * vDiff;
      if (cartesianDiff < minCartesian) {
        minCartesian = cartesianDiff;
        closest = node;
      }
    }

    // Now correct this node based on what we're dragging
    if (closest) {
      let targetBounds = this._lazyStoreGet(closest);
      let farSide = this._dir == "ltr" ? "right" : "left";
      let outsideX = targetBounds[farSide];
      // Check if we're closer to the next target than to this one:
      // Only move if we're not targeting a node in a different row:
      if (aY > targetBounds.top && aY < targetBounds.bottom) {
        if ((this._dir == "ltr" && aX > outsideX) ||
            (this._dir == "rtl" && aX < outsideX)) {
          return closest.nextElementSibling || aContainer;
        }
      }
    }
    return closest;
  },

  /**
   * "Insert" a "placeholder" by shifting the subsequent children out of the
   * way. We go through all the children, and shift them based on the position
   * they would have if we had inserted something before aBefore. We use CSS
   * transforms for this, which are CSS transitioned.
   */
  insertPlaceholder(aContainer, aBefore, aSize, aIsFromThisArea) {
    let isShifted = false;
    for (let child of aContainer.children) {
      // Don't need to shift hidden nodes:
      if (child.getAttribute("hidden") == "true") {
        continue;
      }
      // If this is the node before which we're inserting, start shifting
      // everything that comes after. One exception is inserting at the end
      // of the menupanel, in which case we do not shift the placeholders:
      if (child == aBefore) {
        isShifted = true;
      }
      if (isShifted) {
        if (aIsFromThisArea && !this._lastPlaceholderInsertion) {
          child.setAttribute("notransition", "true");
        }
        // Determine the CSS transform based on the next node:
        child.style.transform = this._diffWithNext(child, aSize);
      } else {
        // If we're not shifting this node, reset the transform
        child.style.transform = "";
      }
    }
    if (aContainer.lastElementChild && aIsFromThisArea &&
        !this._lastPlaceholderInsertion) {
      // Flush layout:
      aContainer.lastElementChild.getBoundingClientRect();
      // then remove all the [notransition]
      for (let child of aContainer.children) {
        child.removeAttribute("notransition");
      }
    }
    this._lastPlaceholderInsertion = aBefore;
  },

  /**
   * Reset all the transforms in this container, optionally without
   * transitioning them.
   * @param aContainer    the container in which to reset transforms
   * @param aNoTransition if truthy, adds a notransition attribute to the node
   *                      while resetting the transform.
   */
  clearPlaceholders(aContainer, aNoTransition) {
    for (let child of aContainer.children) {
      if (aNoTransition) {
        child.setAttribute("notransition", true);
      }
      child.style.transform = "";
      if (aNoTransition) {
        // Need to force a reflow otherwise this won't work.
        child.getBoundingClientRect();
        child.removeAttribute("notransition");
      }
    }
    // We snapped back, so we can assume there's no more
    // "last" placeholder insertion point to keep track of.
    if (aNoTransition) {
      this._lastPlaceholderInsertion = null;
    }
  },

  _diffWithNext(aNode, aSize) {
    let xDiff;
    let yDiff = null;
    let nodeBounds = this._lazyStoreGet(aNode);
    let side = this._dir == "ltr" ? "left" : "right";
    let next = this._getVisibleSiblingForDirection(aNode, "next");
    // First we determine the transform along the x axis.
    // Usually, there will be a next node to base this on:
    if (next) {
      let otherBounds = this._lazyStoreGet(next);
      xDiff = otherBounds[side] - nodeBounds[side];
      // We set this explicitly because otherwise some strange difference
      // between the height and the actual difference between line creeps in
      // and messes with alignments
      yDiff = otherBounds.top - nodeBounds.top;
    } else {
      // We don't have a sibling whose position we can use. First, let's see
      // if we're also the first item (which complicates things):
      let firstNode = this._firstInRow(aNode);
      if (aNode == firstNode) {
        // Maybe we stored the horizontal distance between nodes,
        // if not, we'll use the width of the incoming node as a proxy:
        xDiff = this._horizontalDistance || (this._dir == "ltr" ? 1 : -1) * aSize.width;
      } else {
        // If not, we should be able to get the distance to the previous node
        // and use the inverse, unless there's no room for another node (ie we
        // are the last node and there's no room for another one)
        xDiff = this._moveNextBasedOnPrevious(aNode, nodeBounds, firstNode);
      }
    }

    // If we've not determined the vertical difference yet, check it here
    if (yDiff === null) {
      // If the next node is behind rather than in front, we must have moved
      // vertically:
      if ((xDiff > 0 && this._dir == "rtl") || (xDiff < 0 && this._dir == "ltr")) {
        yDiff = aSize.height;
      } else {
        // Otherwise, we haven't
        yDiff = 0;
      }
    }
    return "translate(" + xDiff + "px, " + yDiff + "px)";
  },

  /**
   * Helper function to find the transform a node if there isn't a next node
   * to base that on.
   * @param aNode           the node to transform
   * @param aNodeBounds     the bounding rect info of this node
   * @param aFirstNodeInRow the first node in aNode's row
   */
  _moveNextBasedOnPrevious(aNode, aNodeBounds, aFirstNodeInRow) {
    let next = this._getVisibleSiblingForDirection(aNode, "previous");
    let otherBounds = this._lazyStoreGet(next);
    let side = this._dir == "ltr" ? "left" : "right";
    let xDiff = aNodeBounds[side] - otherBounds[side];
    // If, however, this means we move outside the container's box
    // (i.e. the row in which this item is placed is full)
    // we should move it to align with the first item in the next row instead
    let bound = this._containerInfo[this._dir == "ltr" ? "right" : "left"];
    if ((this._dir == "ltr" && xDiff + aNodeBounds.right > bound) ||
        (this._dir == "rtl" && xDiff + aNodeBounds.left < bound)) {
      xDiff = this._lazyStoreGet(aFirstNodeInRow)[side] - aNodeBounds[side];
    }
    return xDiff;
  },

  /**
   * Get position details from our cache. If the node is not yet cached, get its position
   * information and cache it now.
   * @param aNode  the node whose position info we want
   * @return the position info
   */
  _lazyStoreGet(aNode) {
    let rect = this._nodePositionStore.get(aNode);
    if (!rect) {
      // getBoundingClientRect() returns a DOMRect that is live, meaning that
      // as the element moves around, the rects values change. We don't want
      // that - we want a snapshot of what the rect values are right at this
      // moment, and nothing else. So we have to clone the values.
      let clientRect = aNode.getBoundingClientRect();
      rect = {
        left: clientRect.left,
        right: clientRect.right,
        width: clientRect.width,
        height: clientRect.height,
        top: clientRect.top,
        bottom: clientRect.bottom,
      };
      rect.x = rect.left + rect.width / 2;
      rect.y = rect.top + rect.height / 2;
      Object.freeze(rect);
      this._nodePositionStore.set(aNode, rect);
    }
    return rect;
  },

  _firstInRow(aNode) {
    // XXXmconley: I'm not entirely sure why we need to take the floor of these
    // values - it looks like, periodically, we're getting fractional pixels back
    // from lazyStoreGet. I've filed bug 994247 to investigate.
    let bound = Math.floor(this._lazyStoreGet(aNode).top);
    let rv = aNode;
    let prev;
    while (rv && (prev = this._getVisibleSiblingForDirection(rv, "previous"))) {
      if (Math.floor(this._lazyStoreGet(prev).bottom) <= bound) {
        return rv;
      }
      rv = prev;
    }
    return rv;
  },

  _getVisibleSiblingForDirection(aNode, aDirection) {
    let rv = aNode;
    do {
      rv = rv[aDirection + "Sibling"];
    } while (rv && rv.getAttribute("hidden") == "true");
    return rv;
  }
};

var DragPositionManager = {
  start(aWindow) {
    let areas = [aWindow.document.getElementById(kPaletteId)];
    for (let areaNode of areas) {
      let positionManager = gManagers.get(areaNode);
      if (positionManager) {
        positionManager.update(areaNode);
      } else {
        gManagers.set(areaNode, new AreaPositionManager(areaNode));
      }
    }
  },

  stop() {
    gManagers = new WeakMap();
  },

  getManagerForArea(aArea) {
    return gManagers.get(aArea);
  }
};

Object.freeze(DragPositionManager);
