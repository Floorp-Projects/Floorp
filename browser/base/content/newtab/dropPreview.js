#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * This singleton provides the ability to re-arrange the current grid to
 * indicate the transformation that results from dropping a cell at a certain
 * position.
 */
var gDropPreview = {
  /**
   * Rearranges the sites currently contained in the grid when a site would be
   * dropped onto the given cell.
   * @param aCell The drop target cell.
   * @return The re-arranged array of sites.
   */
  rearrange: function DropPreview_rearrange(aCell) {
    let sites = gGrid.sites;

    // Insert the dragged site into the current grid.
    this._insertDraggedSite(sites, aCell);

    // After the new site has been inserted we need to correct the positions
    // of all pinned tabs that have been moved around.
    this._repositionPinnedSites(sites, aCell);

    return sites;
  },

  /**
   * Inserts the currently dragged site into the given array of sites.
   * @param aSites The array of sites to insert into.
   * @param aCell The drop target cell.
   */
  _insertDraggedSite: function DropPreview_insertDraggedSite(aSites, aCell) {
    let dropIndex = aCell.index;
    let draggedSite = gDrag.draggedSite;

    // We're currently dragging a site.
    if (draggedSite) {
      let dragCell = draggedSite.cell;
      let dragIndex = dragCell.index;

      // Move the dragged site into its new position.
      if (dragIndex != dropIndex) {
        aSites.splice(dragIndex, 1);
        aSites.splice(dropIndex, 0, draggedSite);
      }
    // We're handling an external drag item.
    } else {
      aSites.splice(dropIndex, 0, null);
    }
  },

  /**
   * Correct the position of all pinned sites that might have been moved to
   * different positions after the dragged site has been inserted.
   * @param aSites The array of sites containing the dragged site.
   * @param aCell The drop target cell.
   */
  _repositionPinnedSites:
    function DropPreview_repositionPinnedSites(aSites, aCell) {

    // Collect all pinned sites.
    let pinnedSites = this._filterPinnedSites(aSites, aCell);

    // Correct pinned site positions.
    pinnedSites.forEach(function (aSite) {
      aSites[aSites.indexOf(aSite)] = aSites[aSite.cell.index];
      aSites[aSite.cell.index] = aSite;
    }, this);

    // There might be a pinned cell that got pushed out of the grid, try to
    // sneak it in by removing a lower-priority cell.
    if (this._hasOverflowedPinnedSite(aSites, aCell))
      this._repositionOverflowedPinnedSite(aSites, aCell);
  },

  /**
   * Filter pinned sites out of the grid that are still on their old positions
   * and have not moved.
   * @param aSites The array of sites to filter.
   * @param aCell The drop target cell.
   * @return The filtered array of sites.
   */
  _filterPinnedSites: function DropPreview_filterPinnedSites(aSites, aCell) {
    let draggedSite = gDrag.draggedSite;

    // When dropping on a cell that contains a pinned site make sure that all
    // pinned cells surrounding the drop target are moved as well.
    let range = this._getPinnedRange(aCell);

    return aSites.filter(function (aSite, aIndex) {
      // The site must be valid, pinned and not the dragged site.
      if (!aSite || aSite == draggedSite || !aSite.isPinned())
        return false;

      let index = aSite.cell.index;

      // If it's not in the 'pinned range' it's a valid pinned site.
      return (index > range.end || index < range.start);
    });
  },

  /**
   * Determines the range of pinned sites surrounding the drop target cell.
   * @param aCell The drop target cell.
   * @return The range of pinned cells.
   */
  _getPinnedRange: function DropPreview_getPinnedRange(aCell) {
    let dropIndex = aCell.index;
    let range = {start: dropIndex, end: dropIndex};

    // We need a pinned range only when dropping on a pinned site.
    if (aCell.containsPinnedSite()) {
      let links = gPinnedLinks.links;

      // Find all previous siblings of the drop target that are pinned as well.
      while (range.start && links[range.start - 1])
        range.start--;

      let maxEnd = links.length - 1;

      // Find all next siblings of the drop target that are pinned as well.
      while (range.end < maxEnd && links[range.end + 1])
        range.end++;
    }

    return range;
  },

  /**
   * Checks if the given array of sites contains a pinned site that has
   * been pushed out of the grid.
   * @param aSites The array of sites to check.
   * @param aCell The drop target cell.
   * @return Whether there is an overflowed pinned cell.
   */
  _hasOverflowedPinnedSite:
    function DropPreview_hasOverflowedPinnedSite(aSites, aCell) {

    // If the drop target isn't pinned there's no way a pinned site has been
    // pushed out of the grid so we can just exit here.
    if (!aCell.containsPinnedSite())
      return false;

    let cells = gGrid.cells;

    // No cells have been pushed out of the grid, nothing to do here.
    if (aSites.length <= cells.length)
      return false;

    let overflowedSite = aSites[cells.length];

    // Nothing to do if the site that got pushed out of the grid is not pinned.
    return (overflowedSite && overflowedSite.isPinned());
  },

  /**
   * We have a overflowed pinned site that we need to re-position so that it's
   * visible again. We try to find a lower-priority cell (empty or containing
   * an unpinned site) that we can move it to.
   * @param aSites The array of sites.
   * @param aCell The drop target cell.
   */
  _repositionOverflowedPinnedSite:
    function DropPreview_repositionOverflowedPinnedSite(aSites, aCell) {

    // Try to find a lower-priority cell (empty or containing an unpinned site).
    let index = this._indexOfLowerPrioritySite(aSites, aCell);

    if (index > -1) {
      let cells = gGrid.cells;
      let dropIndex = aCell.index;

      // Move all pinned cells to their new positions to let the overflowed
      // site fit into the grid.
      for (let i = index + 1, lastPosition = index; i < aSites.length; i++) {
        if (i != dropIndex) {
          aSites[lastPosition] = aSites[i];
          lastPosition = i;
        }
      }

      // Finally, remove the overflowed site from its previous position.
      aSites.splice(cells.length, 1);
    }
  },

  /**
   * Finds the index of the last cell that is empty or contains an unpinned
   * site. These are considered to be of a lower priority.
   * @param aSites The array of sites.
   * @param aCell The drop target cell.
   * @return The cell's index.
   */
  _indexOfLowerPrioritySite:
    function DropPreview_indexOfLowerPrioritySite(aSites, aCell) {

    let cells = gGrid.cells;
    let dropIndex = aCell.index;

    // Search (beginning with the last site in the grid) for a site that is
    // empty or unpinned (an thus lower-priority) and can be pushed out of the
    // grid instead of the pinned site.
    for (let i = cells.length - 1; i >= 0; i--) {
      // The cell that is our drop target is not a good choice.
      if (i == dropIndex)
        continue;

      let site = aSites[i];

      // We can use the cell only if it's empty or the site is un-pinned.
      if (!site || !site.isPinned())
        return i;
    }

    return -1;
  }
};
