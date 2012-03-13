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
let gDropTargetShim = {
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
  init: function DropTargetShim_init() {
    let node = gGrid.node;

    // Add drag event handlers.
    node.addEventListener("dragstart", this, true);
    node.addEventListener("dragend", this, true);
  },

  /**
   * Handles all shim events.
   */
  handleEvent: function DropTargetShim_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "dragstart":
        this._start(aEvent);
        break;
      case "dragover":
        this._dragover(aEvent);
        break;
      case "dragend":
        this._end(aEvent);
        break;
    }
  },

  /**
   * Handles the 'dragstart' event.
   * @param aEvent The 'dragstart' event.
   */
  _start: function DropTargetShim_start(aEvent) {
    if (aEvent.target.classList.contains("newtab-link")) {
      gGrid.lock();

      // XXX bug 505521 - Listen for dragover on the document.
      document.documentElement.addEventListener("dragover", this, false);
    }
  },

  /**
   * Handles the 'drag' event and determines the current drop target.
   * @param aEvent The 'drag' event.
   */
  _drag: function DropTargetShim_drag(aEvent) {
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
   * Handles the 'dragover' event as long as bug 505521 isn't fixed to get
   * current mouse cursor coordinates while dragging.
   * @param aEvent The 'dragover' event.
   */
  _dragover: function DropTargetShim_dragover(aEvent) {
    let sourceNode = aEvent.dataTransfer.mozSourceNode.parentNode;
    gDrag.drag(sourceNode._newtabSite, aEvent);

    this._drag(aEvent);
  },

  /**
   * Handles the 'dragend' event.
   * @param aEvent The 'dragend' event.
   */
  _end: function DropTargetShim_end(aEvent) {
    // Make sure to determine the current drop target in case the dragenter
    // event hasn't been fired.
    this._drag(aEvent);

    if (this._lastDropTarget) {
      if (aEvent.dataTransfer.mozUserCancelled) {
        // The drag operation was cancelled.
        this._dispatchEvent(aEvent, "dragexit", this._lastDropTarget);
        this._dispatchEvent(aEvent, "dragleave", this._lastDropTarget);
      } else {
        // A site was successfully dropped.
        this._dispatchEvent(aEvent, "drop", this._lastDropTarget);
      }

      // Clean up.
      this._lastDropTarget = null;
      this._cellPositions = null;
    }

    gGrid.unlock();

    // XXX bug 505521 - Remove the document's dragover listener.
    document.documentElement.removeEventListener("dragover", this, false);
  },

  /**
   * Determines the current drop target by matching the dragged site's position
   * against all cells in the grid.
   * @return The currently hovered drop target or null.
   */
  _findDropTarget: function DropTargetShim_findDropTarget() {
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
  _dispatchEvent:
    function DropTargetShim_dispatchEvent(aEvent, aType, aTarget) {

    let node = aTarget.node;
    let event = document.createEvent("DragEvents");

    event.initDragEvent(aType, true, true, window, 0, 0, 0, 0, 0, false, false,
                        false, false, 0, node, aEvent.dataTransfer);

    node.dispatchEvent(event);
  }
};
