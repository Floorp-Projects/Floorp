#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * Dialog allowing to undo the removal of single site or to completely restore
 * the grid's original state.
 */
let gUndoDialog = {
  /**
   * The undo dialog's timeout in miliseconds.
   */
  HIDE_TIMEOUT_MS: 15000,

  /**
   * Contains undo information.
   */
  _undoData: null,

  /**
   * Initializes the undo dialog.
   */
  init: function UndoDialog_init() {
    this._undoContainer = document.getElementById("newtab-undo-container");
    this._undoContainer.addEventListener("click", this, false);
    this._undoButton = document.getElementById("newtab-undo-button");
    this._undoCloseButton = document.getElementById("newtab-undo-close-button");
    this._undoRestoreButton = document.getElementById("newtab-undo-restore-button");
  },

  /**
   * Shows the undo dialog.
   * @param aSite The site that just got removed.
   */
  show: function UndoDialog_show(aSite) {
    if (this._undoData)
      clearTimeout(this._undoData.timeout);

    this._undoData = {
      index: aSite.cell.index,
      wasPinned: aSite.isPinned(),
      blockedLink: aSite.link,
      timeout: setTimeout(this.hide.bind(this), this.HIDE_TIMEOUT_MS)
    };

    this._undoContainer.removeAttribute("undo-disabled");
    this._undoButton.removeAttribute("tabindex");
    this._undoCloseButton.removeAttribute("tabindex");
    this._undoRestoreButton.removeAttribute("tabindex");
  },

  /**
   * Hides the undo dialog.
   */
  hide: function UndoDialog_hide() {
    if (!this._undoData)
      return;

    clearTimeout(this._undoData.timeout);
    this._undoData = null;
    this._undoContainer.setAttribute("undo-disabled", "true");
    this._undoButton.setAttribute("tabindex", "-1");
    this._undoCloseButton.setAttribute("tabindex", "-1");
    this._undoRestoreButton.setAttribute("tabindex", "-1");
  },

  /**
   * The undo dialog event handler.
   * @param aEvent The event to handle.
   */
  handleEvent: function UndoDialog_handleEvent(aEvent) {
    switch (aEvent.target.id) {
      case "newtab-undo-button":
        this._undo();
        break;
      case "newtab-undo-restore-button":
        this._undoAll();
        break;
      case "newtab-undo-close-button":
        this.hide();
        break;
    }
  },

  /**
   * Undo the last blocked site.
   */
  _undo: function UndoDialog_undo() {
    if (!this._undoData)
      return;

    let {index, wasPinned, blockedLink} = this._undoData;
    gBlockedLinks.unblock(blockedLink);

    if (wasPinned) {
      gPinnedLinks.pin(blockedLink, index);
    }

    gUpdater.updateGrid();
    this.hide();
  },

  /**
   * Undo all blocked sites.
   */
  _undoAll: function UndoDialog_undoAll() {
    NewTabUtils.undoAll(function() {
      gUpdater.updateGrid();
      this.hide();
    }.bind(this));
  }
};

gUndoDialog.init();
