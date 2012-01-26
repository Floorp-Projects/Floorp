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
  get cells() {
    let children = this.node.querySelectorAll("li");
    let cells = [new Cell(this, child) for each (child in children)];

    // Replace the getter with our cached value.
    Object.defineProperty(this, "cells", {value: cells, enumerable: true});

    return cells;
  },

  /**
   * All sites contained in the grid's cells. Sites may be empty.
   */
  get sites() [cell.site for each (cell in this.cells)],

  /**
   * Initializes the grid.
   * @param aSelector The query selector of the grid.
   */
  init: function Grid_init(aSelector) {
    this._node = document.querySelector(aSelector);
    this._createSiteFragment();
    this._draw();
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

    // Draw the grid again.
    this._draw();
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
   * Creates the DOM fragment that is re-used when creating sites.
   */
  _createSiteFragment: function Grid_createSiteFragment() {
    let site = document.createElementNS(HTML_NAMESPACE, "a");
    site.classList.add("site");
    site.setAttribute("draggable", "true");

    // Create the site's inner HTML code.
    site.innerHTML =
      '<img class="site-img" width="' + THUMB_WIDTH +'" ' +
      ' height="' + THUMB_HEIGHT + '" alt=""/>' +
      '<span class="site-title"/>' +
      '<span class="site-strip">' +
      '  <input class="button strip-button strip-button-pin" type="button"' +
      '   tabindex="-1" title="' + newTabString("pin") + '"/>' +
      '  <input class="button strip-button strip-button-block" type="button"' +
      '   tabindex="-1" title="' + newTabString("block") + '"/>' +
      '</span>';

    this._siteFragment = document.createDocumentFragment();
    this._siteFragment.appendChild(site);
  },

  /**
   * Draws the grid, creates all sites and puts them into their cells.
   */
  _draw: function Grid_draw() {
    let cells = this.cells;

    // Put sites into the cells.
    let links = gLinks.getLinks();
    let length = Math.min(links.length, cells.length);

    for (let i = 0; i < length; i++) {
      if (links[i])
        this.createSite(links[i], cells[i]);
    }
  }
};
