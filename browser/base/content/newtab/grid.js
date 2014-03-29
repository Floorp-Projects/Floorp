#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * Define various fixed dimensions
 */
const GRID_BOTTOM_EXTRA = 4; // title's line-height extends 4px past the margin
const GRID_WIDTH_EXTRA = 1; // provide 1px buffer to allow for rounding error

/**
 * This singleton represents the grid that contains all sites.
 */
let gGrid = {
  /**
   * The DOM node of the grid.
   */
  _node: null,
  get node() this._node,

  /**
   * The cached DOM fragment for sites.
   */
  _siteFragment: null,

  /**
   * All cells contained in the grid.
   */
  _cells: null,
  get cells() this._cells,

  /**
   * All sites contained in the grid's cells. Sites may be empty.
   */
  get sites() [cell.site for each (cell in this.cells)],

  // Tells whether the grid has already been initialized.
  get ready() !!this._node,

  /**
   * Initializes the grid.
   * @param aSelector The query selector of the grid.
   */
  init: function Grid_init() {
    this._node = document.getElementById("newtab-grid");
    this._createSiteFragment();
    this._render();
    addEventListener("load", this);
    addEventListener("resize", this);
  },

  /**
   * Creates a new site in the grid.
   * @param aLink The new site's link.
   * @param aCell The cell that will contain the new site.
   * @return The newly created site.
   */
  createSite: function Grid_createSite(aLink, aCell) {
    let node = aCell.node;
    node.appendChild(this._siteFragment.cloneNode(true));
    return new Site(node.firstElementChild, aLink);
  },

  /**
   * Handles all grid events.
   */
  handleEvent: function Grid_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "load":
        // Save the cell's computed height/width including margin and border
        let refCell = document.querySelector(".newtab-cell");
        this._cellMargin = parseFloat(getComputedStyle(refCell).marginTop) * 2;
        this._cellHeight = refCell.offsetHeight + this._cellMargin;
        this._cellWidth = refCell.offsetWidth + this._cellMargin;
        this._resizeGrid();
        break;

      case "resize":
        this._resizeGrid();
        break;
    }
  },

  /**
   * Refreshes the grid and re-creates all sites.
   */
  refresh: function Grid_refresh() {
    // Remove all sites.
    this.cells.forEach(function (cell) {
      let node = cell.node;
      let child = node.firstElementChild;

      if (child)
        node.removeChild(child);
    }, this);

    // Render the grid again.
    this._render();
    this._resizeGrid();
  },

  /**
   * Locks the grid to block all pointer events.
   */
  lock: function Grid_lock() {
    this.node.setAttribute("locked", "true");
  },

  /**
   * Unlocks the grid to allow all pointer events.
   */
  unlock: function Grid_unlock() {
    this.node.removeAttribute("locked");
  },

  /**
   * Creates the newtab grid.
   */
  _renderGrid: function Grid_renderGrid() {
    let cell = document.createElementNS(HTML_NAMESPACE, "div");
    cell.classList.add("newtab-cell");

    // Clear the grid
    this._node.innerHTML = "";

    // Creates all the cells up to the maximum
    for (let i = 0; i < gGridPrefs.gridColumns * gGridPrefs.gridRows; i++) {
      this._node.appendChild(cell.cloneNode(true));
    }

    // (Re-)initialize all cells.
    let cellElements = this.node.querySelectorAll(".newtab-cell");
    this._cells = [new Cell(this, cell) for (cell of cellElements)];
  },

  /**
   * Calculate the height for a number of rows up to the maximum rows
   * @param rows Number of rows defaulting to the max
   */
  _computeHeight: function Grid_computeHeight(aRows) {
    let {gridRows} = gGridPrefs;
    aRows = aRows === undefined ? gridRows : Math.min(gridRows, aRows);
    return aRows * this._cellHeight + GRID_BOTTOM_EXTRA;
  },

  /**
   * Creates the DOM fragment that is re-used when creating sites.
   */
  _createSiteFragment: function Grid_createSiteFragment() {
    let site = document.createElementNS(HTML_NAMESPACE, "div");
    site.classList.add("newtab-site");
    site.setAttribute("draggable", "true");

    // Create the site's inner HTML code.
    site.innerHTML =
      '<a class="newtab-link">' +
      '  <span class="newtab-thumbnail"/>' +
      '  <span class="newtab-title"/>' +
      '</a>' +
      '<input type="button" title="' + newTabString("pin") + '"' +
      '       class="newtab-control newtab-control-pin"/>' +
      '<input type="button" title="' + newTabString("block") + '"' +
      '       class="newtab-control newtab-control-block"/>' +
      '<input type="button" title="' + newTabString("sponsored") + '"' +
      '       class="newtab-control newtab-control-sponsored"/>';

    this._siteFragment = document.createDocumentFragment();
    this._siteFragment.appendChild(site);
  },

  /**
   * Renders the sites, creates all sites and puts them into their cells.
   */
  _renderSites: function Grid_renderSites() {
    let cells = this.cells;
    // Put sites into the cells.
    let links = gLinks.getLinks();
    let length = Math.min(links.length, cells.length);

    for (let i = 0; i < length; i++) {
      if (links[i])
        this.createSite(links[i], cells[i]);
    }
  },

  /**
   * Renders the grid.
   */
  _render: function Grid_render() {
    if (this._shouldRenderGrid()) {
      this._renderGrid();
    }

    this._renderSites();
  },

  /**
   * Make sure the correct number of rows and columns are visible
   */
  _resizeGrid: function Grid_resizeGrid() {
    let availSpace = document.documentElement.clientHeight - this._cellMargin -
                     document.querySelector("#newtab-margin-top").offsetHeight;
    let visibleRows = Math.floor(availSpace / this._cellHeight);
    this._node.style.height = this._computeHeight() + "px";
    this._node.style.maxHeight = this._computeHeight(visibleRows) + "px";
    this._node.style.maxWidth = gGridPrefs.gridColumns * this._cellWidth +
                                GRID_WIDTH_EXTRA + "px";
  },

  _shouldRenderGrid : function Grid_shouldRenderGrid() {
    let cellsLength = this._node.querySelectorAll(".newtab-cell").length;
    return cellsLength != (gGridPrefs.gridRows * gGridPrefs.gridColumns);
  }
};
