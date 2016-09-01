#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * This singleton provides a custom drop target detection. We need this because
 * the default DnD target detection relies on the cursor's position. We want
 * to pick a drop target based on the dragged site's position.
 */
var gDropTargetShim = {
  /**
   * Cache for the position of all cells, cleaned after drag finished.
   */
  _cellPositions: null,

  /**
   * The last drop target that was hovered.
   */
  _lastDropTarget: null,

  /**
   * Initializes the drop target shim.
   */
  init: function () {
    gGrid.node.addEventListener("dragstart", this, true);
  },

  /**
   * Add all event listeners needed during a drag operation.
   */
  _addEventListeners: function () {
    gGrid.node.addEventListener("dragend", this);

    let docElement = document.documentElement;
    docElement.addEventListener("dragover", this);
    docElement.addEventListener("dragenter", this);
    docElement.addEventListener("drop", this);
  },

  /**
   * Remove all event listeners that were needed during a drag operation.
   */
  _removeEventListeners: function () {
    gGrid.node.removeEventListener("dragend", this);

    let docElement = document.documentElement;
    docElement.removeEventListener("dragover", this);
    docElement.removeEventListener("dragenter", this);
    docElement.removeEventListener("drop", this);
  },

  /**
   * Handles all shim events.
   */
  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "dragstart":
        this._dragstart(aEvent);
        break;
      case "dragenter":
        aEvent.preventDefault();
        break;
      case "dragover":
        this._dragover(aEvent);
        break;
      case "drop":
        this._drop(aEvent);
        break;
      case "dragend":
        this._dragend(aEvent);
        break;
    }
  },

  /**
   * Handles the 'dragstart' event.
   * @param aEvent The 'dragstart' event.
   */
  _dragstart: function (aEvent) {
    if (aEvent.target.classList.contains("newtab-link")) {
      gGrid.lock();
      this._addEventListeners();
    }
  },

  /**
   * Handles the 'dragover' event.
   * @param aEvent The 'dragover' event.
   */
  _dragover: function (aEvent) {
    // XXX bug 505521 - Use the dragover event to retrieve the
    //                  current mouse coordinates while dragging.
    let sourceNode = aEvent.dataTransfer.mozSourceNode.parentNode;
    gDrag.drag(sourceNode._newtabSite, aEvent);

    // Find the current drop target, if there's one.
    this._updateDropTarget(aEvent);

    // If we have a valid drop target,
    // let the drag-and-drop service know.
    if (this._lastDropTarget) {
      aEvent.preventDefault();
    }
  },

  /**
   * Handles the 'drop' event.
   * @param aEvent The 'drop' event.
   */
  _drop: function (aEvent) {
    // We're accepting all drops.
    aEvent.preventDefault();

    // remember that drop event was seen, this explicitly
    // assumes that drop event preceeds dragend event
    this._dropSeen = true;

    // Make sure to determine the current drop target
    // in case the dragover event hasn't been fired.
    this._updateDropTarget(aEvent);

    // A site was successfully dropped.
    this._dispatchEvent(aEvent, "drop", this._lastDropTarget);
  },

  /**
   * Handles the 'dragend' event.
   * @param aEvent The 'dragend' event.
   */
  _dragend: function (aEvent) {
    if (this._lastDropTarget) {
      if (aEvent.dataTransfer.mozUserCancelled || !this._dropSeen) {
        // The drag operation was cancelled or no drop event was generated
        this._dispatchEvent(aEvent, "dragexit", this._lastDropTarget);
        this._dispatchEvent(aEvent, "dragleave", this._lastDropTarget);
      }

      // Clean up.
      this._lastDropTarget = null;
      this._cellPositions = null;
    }

    this._dropSeen = false;
    gGrid.unlock();
    this._removeEventListeners();
  },

  /**
   * Tries to find the current drop target and will fire
   * appropriate dragenter, dragexit, and dragleave events.
   * @param aEvent The current drag event.
   */
  _updateDropTarget: function (aEvent) {
    // Let's see if we find a drop target.
    let target = this._findDropTarget(aEvent);

    if (target != this._lastDropTarget) {
      if (this._lastDropTarget)
        // We left the last drop target.
        this._dispatchEvent(aEvent, "dragexit", this._lastDropTarget);

      if (target)
        // We're now hovering a (new) drop target.
        this._dispatchEvent(aEvent, "dragenter", target);

      if (this._lastDropTarget)
        // We left the last drop target.
        this._dispatchEvent(aEvent, "dragleave", this._lastDropTarget);

      this._lastDropTarget = target;
    }
  },

  /**
   * Determines the current drop target by matching the dragged site's position
   * against all cells in the grid.
   * @return The currently hovered drop target or null.
   */
  _findDropTarget: function () {
    // These are the minimum intersection values - we want to use the cell if
    // the site is >= 50% hovering its position.
    let minWidth = gDrag.cellWidth / 2;
    let minHeight = gDrag.cellHeight / 2;

    let cellPositions = this._getCellPositions();
    let rect = gTransformation.getNodePosition(gDrag.draggedSite.node);

    // Compare each cell's position to the dragged site's position.
    for (let i = 0; i < cellPositions.length; i++) {
      let inter = rect.intersect(cellPositions[i].rect);

      // If the intersection is big enough we found a drop target.
      if (inter.width >= minWidth && inter.height >= minHeight)
        return cellPositions[i].cell;
    }

    // No drop target found.
    return null;
  },

  /**
   * Gets the positions of all cell nodes.
   * @return The (cached) cell positions.
   */
  _getCellPositions: function DropTargetShim_getCellPositions() {
    if (this._cellPositions)
      return this._cellPositions;

    return this._cellPositions = gGrid.cells.map(function (cell) {
      return {cell: cell, rect: gTransformation.getNodePosition(cell.node)};
    });
  },

  /**
   * Dispatches a custom DragEvent on the given target node.
   * @param aEvent The source event.
   * @param aType The event type.
   * @param aTarget The target node that receives the event.
   */
  _dispatchEvent: function (aEvent, aType, aTarget) {
    let node = aTarget.node;
    let event = document.createEvent("DragEvent");

    // The event should not bubble to prevent recursion.
    event.initDragEvent(aType, false, true, window, 0, 0, 0, 0, 0, false, false,
                        false, false, 0, node, aEvent.dataTransfer);

    node.dispatchEvent(event);
  }
};
