#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * This class manages a cell's DOM node (not the actually cell content, a site).
 * It's mostly read-only, i.e. all manipulation of both position and content
 * aren't handled here.
 */
function Cell(aGrid, aNode) {
  this._grid = aGrid;
  this._node = aNode;
  this._node._newtabCell = this;

  // Register drag-and-drop event handlers.
  ["DragEnter", "DragOver", "DragExit", "Drop"].forEach(function (aType) {
    let method = "on" + aType;
    this[method] = this[method].bind(this);
    this._node.addEventListener(aType.toLowerCase(), this[method], false);
  }, this);
}

Cell.prototype = {
  /**
   *
   */
  _grid: null,

  /**
   * The cell's DOM node.
   */
  get node() this._node,

  /**
   * The cell's offset in the grid.
   */
  get index() {
    let index = this._grid.cells.indexOf(this);

    // Cache this value, overwrite the getter.
    Object.defineProperty(this, "index", {value: index, enumerable: true});

    return index;
  },

  /**
   * The previous cell in the grid.
   */
  get previousSibling() {
    let prev = this.node.previousElementSibling;
    prev = prev && prev._newtabCell;

    // Cache this value, overwrite the getter.
    Object.defineProperty(this, "previousSibling", {value: prev, enumerable: true});

    return prev;
  },

  /**
   * The next cell in the grid.
   */
  get nextSibling() {
    let next = this.node.nextElementSibling;
    next = next && next._newtabCell;

    // Cache this value, overwrite the getter.
    Object.defineProperty(this, "nextSibling", {value: next, enumerable: true});

    return next;
  },

  /**
   * The site contained in the cell, if any.
   */
  get site() {
    let firstChild = this.node.firstElementChild;
    return firstChild && firstChild._newtabSite;
  },

  /**
   * Checks whether the cell contains a pinned site.
   * @return Whether the cell contains a pinned site.
   */
  containsPinnedSite: function Cell_containsPinnedSite() {
    let site = this.site;
    return site && site.isPinned();
  },

  /**
   * Checks whether the cell contains a site (is empty).
   * @return Whether the cell is empty.
   */
  isEmpty: function Cell_isEmpty() {
    return !this.site;
  },

  /**
   * Event handler for the 'dragenter' event.
   * @param aEvent The dragenter event.
   */
  onDragEnter: function Cell_onDragEnter(aEvent) {
    if (gDrag.isValid(aEvent)) {
      aEvent.preventDefault();
      gDrop.enter(this, aEvent);
    }
  },

  /**
   * Event handler for the 'dragover' event.
   * @param aEvent The dragover event.
   */
  onDragOver: function Cell_onDragOver(aEvent) {
    if (gDrag.isValid(aEvent))
      aEvent.preventDefault();
  },

  /**
   * Event handler for the 'dragexit' event.
   * @param aEvent The dragexit event.
   */
  onDragExit: function Cell_onDragExit(aEvent) {
    gDrop.exit(this, aEvent);
  },

  /**
   * Event handler for the 'drop' event.
   * @param aEvent The drop event.
   */
  onDrop: function Cell_onDrop(aEvent) {
    if (gDrag.isValid(aEvent)) {
      aEvent.preventDefault();
      gDrop.drop(this, aEvent);
    }
  }
};
