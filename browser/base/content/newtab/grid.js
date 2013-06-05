#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

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
    let row = document.createElementNS(HTML_NAMESPACE, "div");
    let cell = document.createElementNS(HTML_NAMESPACE, "div");
    row.classList.add("newtab-row");
    cell.classList.add("newtab-cell");

    // Clear the grid
    this._node.innerHTML = "";

    // Creates the structure of one row
    for (let i = 0; i < gGridPrefs.gridColumns; i++) {
      row.appendChild(cell.cloneNode(true));
    }
    // Creates the grid
    for (let j = 0; j < gGridPrefs.gridRows; j++) {
      this._node.appendChild(row.cloneNode(true));
    }

    // (Re-)initialize all cells.
    let cellElements = this.node.querySelectorAll(".newtab-cell");
    this._cells = [new Cell(this, cell) for (cell of cellElements)];
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
      '       class="newtab-control newtab-control-block"/>';

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

  _shouldRenderGrid : function Grid_shouldRenderGrid() {
    let rowsLength = this._node.querySelectorAll(".newtab-row").length;
    let cellsLength = this._node.querySelectorAll(".newtab-cell").length;

    return (rowsLength != gGridPrefs.gridRows ||
            cellsLength != (gGridPrefs.gridRows * gGridPrefs.gridColumns));
  }
};
