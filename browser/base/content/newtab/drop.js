#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

// A little delay that prevents the grid from being too sensitive when dragging
// sites around.
const DELAY_REARRANGE_MS = 100;

/**
 * This singleton implements site dropping functionality.
 */
let gDrop = {
  /**
   * The last drop target.
   */
  _lastDropTarget: null,

  /**
   * Handles the 'dragenter' event.
   * @param aCell The drop target cell.
   */
  enter: function Drop_enter(aCell) {
    this._delayedRearrange(aCell);
  },

  /**
   * Handles the 'dragexit' event.
   * @param aCell The drop target cell.
   * @param aEvent The 'dragexit' event.
   */
  exit: function Drop_exit(aCell, aEvent) {
    if (aEvent.dataTransfer && !aEvent.dataTransfer.mozUserCancelled) {
      this._delayedRearrange();
    } else {
      // The drag operation has been cancelled.
      this._cancelDelayedArrange();
      this._rearrange();
    }
  },

  /**
   * Handles the 'drop' event.
   * @param aCell The drop target cell.
   * @param aEvent The 'dragexit' event.
   */
  drop: function Drop_drop(aCell, aEvent) {
    // The cell that is the drop target could contain a pinned site. We need
    // to find out where that site has gone and re-pin it there.
    if (aCell.containsPinnedSite())
      this._repinSitesAfterDrop(aCell);

    // Pin the dragged or insert the new site.
    this._pinDraggedSite(aCell, aEvent);

    this._cancelDelayedArrange();

    // Update the grid and move all sites to their new places.
    gUpdater.updateGrid(gDrag.draggedSite);
  },

  /**
   * Re-pins all pinned sites in their (new) positions.
   * @param aCell The drop target cell.
   */
  _repinSitesAfterDrop: function Drop_repinSitesAfterDrop(aCell) {
    let sites = gDropPreview.rearrange(aCell);

    // Filter out pinned sites.
    let pinnedSites = sites.filter(function (aSite) {
      return aSite && aSite.isPinned();
    });

    // Re-pin all shifted pinned cells.
    pinnedSites.forEach(function (aSite) aSite.pin(sites.indexOf(aSite)), this);
  },

  /**
   * Pins the dragged site in its new place.
   * @param aCell The drop target cell.
   * @param aEvent The 'dragexit' event.
   */
  _pinDraggedSite: function Drop_pinDraggedSite(aCell, aEvent) {
    let index = aCell.index;
    let draggedSite = gDrag.draggedSite;

    if (draggedSite) {
      // Pin the dragged site at its new place.
      if (aCell != draggedSite.cell)
        draggedSite.pin(index);
    } else {
      let link = gDragDataHelper.getLinkFromDragEvent(aEvent);
      if (link) {
        // A new link was dragged onto the grid. Create it by pinning its URL.
        gPinnedLinks.pin(link, index);

        // Make sure the newly added link is not blocked.
        gBlockedLinks.unblock(link);
      }
    }
  },

  /**
   * Time a rearrange with a little delay.
   * @param aCell The drop target cell.
   */
  _delayedRearrange: function Drop_delayedRearrange(aCell) {
    // The last drop target didn't change so there's no need to re-arrange.
    if (this._lastDropTarget == aCell)
      return;

    let self = this;

    function callback() {
      self._rearrangeTimeout = null;
      self._rearrange(aCell);
    }

    this._cancelDelayedArrange();
    this._rearrangeTimeout = setTimeout(callback, DELAY_REARRANGE_MS);

    // Store the last drop target.
    this._lastDropTarget = aCell;
  },

  /**
   * Cancels a timed rearrange, if any.
   */
  _cancelDelayedArrange: function Drop_cancelDelayedArrange() {
    if (this._rearrangeTimeout) {
      clearTimeout(this._rearrangeTimeout);
      this._rearrangeTimeout = null;
    }
  },

  /**
   * Rearrange all sites in the grid depending on the current drop target.
   * @param aCell The drop target cell.
   */
  _rearrange: function Drop_rearrange(aCell) {
    let sites = gGrid.sites;

    // We need to rearrange the grid only if there's a current drop target.
    if (aCell)
      sites = gDropPreview.rearrange(aCell);

    gTransformation.rearrangeSites(sites, gDrag.draggedSite, {unfreeze: !aCell});
  }
};
