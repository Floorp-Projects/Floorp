#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * Define various fixed dimensions
 */
const GRID_BOTTOM_EXTRA = 7; // title's line-height extends 7px past the margin
const GRID_WIDTH_EXTRA = 1; // provide 1px buffer to allow for rounding error
const SPONSORED_TAG_BUFFER = 2; // 2px buffer to clip off top of sponsored tag

/**
 * This singleton represents the grid that contains all sites.
 */
var gGrid = {
  /**
   * The DOM node of the grid.
   */
  _node: null,
  get node() { return this._node; },

  /**
   * The cached DOM fragment for sites.
   */
  _siteFragment: null,

  /**
   * All cells contained in the grid.
   */
  _cells: [],
  get cells() { return this._cells; },

  /**
   * All sites contained in the grid's cells. Sites may be empty.
   */
  get sites() { return [for (cell of this.cells) cell.site]; },

  // Tells whether the grid has already been initialized.
  get ready() { return !!this._ready; },

  // Returns whether the page has finished loading yet.
  get isDocumentLoaded() { return document.readyState == "complete"; },

  /**
   * Initializes the grid.
   * @param aSelector The query selector of the grid.
   */
  init: function Grid_init() {
    this._node = document.getElementById("newtab-grid");
    this._createSiteFragment();

    gLinks.populateCache(() => {
      this._refreshGrid();
      this._ready = true;

      // If fetching links took longer than loading the page itself then
      // we need to resize the grid as that was blocked until now.
      // We also want to resize now if the page was already loaded when
      // initializing the grid (the user toggled the page).
      this._resizeGrid();

      addEventListener("resize", this);
    });

    // Resize the grid as soon as the page loads.
    if (!this.isDocumentLoaded) {
      addEventListener("load", this);
    }
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
      case "resize":
        this._resizeGrid();
        break;
    }
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
   * Renders and resizes the gird. _resizeGrid() call is needed to ensure
   * that scrollbar disappears when the bottom row becomes empty following
   * the block action, or tile display is turmed off via cog menu
   */

  refresh() {
    this._refreshGrid();
    this._resizeGrid();
  },

  /**
   * Renders the grid, including cells and sites.
   */
  _refreshGrid() {
    let cell = document.createElementNS(HTML_NAMESPACE, "div");
    cell.classList.add("newtab-cell");

    // Creates all the cells up to the maximum
    let fragment = document.createDocumentFragment();
    for (let i = 0; i < gGridPrefs.gridColumns * gGridPrefs.gridRows; i++) {
      fragment.appendChild(cell.cloneNode(true));
    }

    // Create cells.
    let cells = Array.from(fragment.childNodes, (cell) => new Cell(this, cell));

    // Fetch links.
    let links = gLinks.getLinks();

    // Create sites.
    let numLinks = Math.min(links.length, cells.length);
    for (let i = 0; i < numLinks; i++) {
      if (links[i]) {
        this.createSite(links[i], cells[i]);
      }
    }

    this._cells = cells;
    this._node.innerHTML = "";
    this._node.appendChild(fragment);
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
      '<span class="newtab-sponsored">' + newTabString("sponsored.button") + '</span>' +
      '<a class="newtab-link">' +
      '  <span class="newtab-thumbnail"/>' +
      '  <span class="newtab-thumbnail enhanced-content"/>' +
      '  <span class="newtab-title"/>' +
      '</a>' +
      '<input type="button" title="' + newTabString("pin") + '"' +
      '       class="newtab-control newtab-control-pin"/>' +
      '<input type="button" title="' + newTabString("block") + '"' +
      '       class="newtab-control newtab-control-block"/>' +
      '<span class="newtab-suggested"/>';

    this._siteFragment = document.createDocumentFragment();
    this._siteFragment.appendChild(site);
  },

  /**
   * Test a tile at a given position for being pinned or history
   * @param position Position in sites array
   */
  _isHistoricalTile: function Grid_isHistoricalTile(aPos) {
    let site = this.sites[aPos];
    return site && (site.isPinned() || site.link && site.link.type == "history");
  },

  /**
   * Make sure the correct number of rows and columns are visible
   */
  _resizeGrid: function Grid_resizeGrid() {
    // If we're somehow called before the page has finished loading,
    // let's bail out to avoid caching zero heights and widths.
    // We'll be called again when DOMContentLoaded fires.
    // Same goes for the grid if that's not ready yet.
    if (!this.isDocumentLoaded || !this._ready) {
      return;
    }

    // Save the cell's computed height/width including margin and border
    if (this._cellMargin === undefined) {
      let refCell = document.querySelector(".newtab-cell");
      this._cellMargin = parseFloat(getComputedStyle(refCell).marginTop);
      this._cellHeight = refCell.offsetHeight + this._cellMargin +
        parseFloat(getComputedStyle(refCell).marginBottom);
      this._cellWidth = refCell.offsetWidth + this._cellMargin;
    }

    let searchContainer = document.querySelector("#newtab-search-container");
    // Save search-container margin height
    if (this._searchContainerMargin  === undefined) {
      this._searchContainerMargin = parseFloat(getComputedStyle(searchContainer).marginBottom) +
                                    parseFloat(getComputedStyle(searchContainer).marginTop);
    }

    // Find the number of rows we can place into view port
    let availHeight = document.documentElement.clientHeight - this._cellMargin -
                      searchContainer.offsetHeight - this._searchContainerMargin;
    let visibleRows = Math.floor(availHeight / this._cellHeight);

    // Find the number of columns that fit into view port
    let maxGridWidth = gGridPrefs.gridColumns * this._cellWidth + GRID_WIDTH_EXTRA;
    // available width is current grid width, but no greater than maxGridWidth
    let availWidth = Math.min(document.querySelector("#newtab-grid").clientWidth,
                              maxGridWidth);
    // finally get the number of columns we can fit into view port
    let gridColumns =  Math.floor(availWidth / this._cellWidth);
    // walk sites backwords until a pinned or history tile is found or visibleRows reached
    let tileIndex = Math.min(gGridPrefs.gridRows * gridColumns, this.sites.length) - 1;
    while (tileIndex >= visibleRows * gridColumns) {
      if (this._isHistoricalTile(tileIndex)) {
        break;
      }
      tileIndex --;
    }

    // Compute the actual number of grid rows we will display (potentially
    // with a scroll bar). tileIndex now points to a historical tile with
    // heighest index or to the last index of the visible row, if none found
    // Dividing tileIndex by number of tiles in a column gives the rows
    let gridRows = Math.floor(tileIndex / gridColumns) + 1;

    // we need to set grid width, for otherwise the scrollbar may shrink
    // the grid when shown and cause grid layout to be different from
    // what being computed above. This, in turn, may cause scrollbar shown
    // for directory tiles, and introduce jitter when grid width is aligned
    // exactly on the column boundary
    this._node.style.width = gridColumns * this._cellWidth + "px";
    this._node.style.maxWidth = gGridPrefs.gridColumns * this._cellWidth +
                                GRID_WIDTH_EXTRA + "px";
    this._node.style.height = this._computeHeight() + "px";
    this._node.style.maxHeight = this._computeHeight(gridRows) - SPONSORED_TAG_BUFFER + "px";
  }
};
