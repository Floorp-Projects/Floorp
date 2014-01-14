// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* We don't support zooming yet, disable Animated zoom by clamping it to the default zoom. */
const kBrowserFindZoomLevelMin = 1;
const kBrowserFindZoomLevelMax = 1;

var FindHelperUI = {
  type: "find",
  commands: {
    next: "cmd_findNext",
    previous: "cmd_findPrevious",
    close: "cmd_findClose"
  },

  _finder: null,
  _open: false,
  _status: null,
  _searchString: "",

  /*
   * Properties
   */

  get isActive() {
    return this._open;
  },

  get status() {
    return this._status;
  },

  set status(val) {
    if (val != this._status) {
      this._status = val;
      if (!val)
        this._textbox.removeAttribute("status");
      else
        this._textbox.setAttribute("status", val);
      this.updateCommands(this._textbox.value);
    }
  },

  init: function findHelperInit() {
    this._textbox = document.getElementById("findbar-textbox");
    this._container = Elements.findbar;

    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    this._textbox.addEventListener("keydown", this);

    // Listen for events where form assistant should be closed
    Elements.tabList.addEventListener("TabSelect", this, true);
    Elements.browsers.addEventListener("URLChanged", this, true);
    window.addEventListener("MozAppbarShowing", this);
    window.addEventListener("MozFlyoutPanelShowing", this, false);
  },

  handleEvent: function findHelperHandleEvent(aEvent) {
    switch (aEvent.type) {
      case "TabSelect":
        this.hide();
        break;

      case "URLChanged":
        if (aEvent.detail && aEvent.target == getBrowser())
          this.hide();
        break;

      case "keydown":
        if (aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN) {
          let backwardsSearch = aEvent.shiftKey;
          this.searchAgain(this._searchString, backwardsSearch);
        }
        break;

      case "MozAppbarShowing":
      case "MozFlyoutPanelShowing":
        if (aEvent.target != this._container) {
          this.hide();
        }
        break;
    }
  },

  show: function findHelperShow() {
    if (BrowserUI.isStartTabVisible) {
      return;
    }
    if (this._open) {
      setTimeout(() => {
        this._textbox.select();
        this._textbox.focus();
      }, 0);
      return;
    }

    // Hide any menus
    ContextUI.dismiss();

    // Shutdown selection related ui
    SelectionHelperUI.closeEditSession();

    let findbar = this._container;
    setTimeout(() => {
      findbar.show();
      this.search(this._textbox.value);
      this._textbox.select();
      this._textbox.focus();

      this._open = true;
    }, 0);

    // Prevent the view to scroll automatically while searching
    Browser.selectedBrowser.scrollSync = false;
  },

  hide: function findHelperHide() {
    if (!this._open)
      return;

    ContentAreaObserver.shiftBrowserDeck(0);

    let onTransitionEnd = () => {
      this._container.removeEventListener("transitionend", onTransitionEnd, true);
      this._textbox.value = "";
      this.status = null;
      this._open = false;
      if (this._finder) {
        this._finder.removeResultListener(this);
        this._finder = null
      }
      // Restore the scroll synchronisation
      Browser.selectedBrowser.scrollSync = true;
    };

    this._textbox.blur();
    this._container.addEventListener("transitionend", onTransitionEnd, true);
    this._container.dismiss();
  },

  search: function findHelperSearch(aValue) {
    if (!this._finder) {
      this._finder = Browser.selectedBrowser.finder;
      this._finder.addResultListener(this);
    }
    this._searchString = aValue;
    if (aValue != "") {
      this._finder.fastFind(aValue, false, false);
    } else {
      this.updateCommands();
    }
  },

  searchAgain: function findHelperSearchAgain(aValue, aFindBackwards) {
    // This can happen if the user taps next/previous after re-opening the search bar
    if (!this._finder) {
      this.search(aValue);
      return;
    }

    this._finder.findAgain(aFindBackwards, false, false);
  },

  goToPrevious: function findHelperGoToPrevious() {
    this.searchAgain(this._searchString, true);
  },

  goToNext: function findHelperGoToNext() {
    this.searchAgain(this._searchString, false);
  },

  onFindResult: function(aData) {
    this._status = aData.result;
    if (aData.rect) {
      this._zoom(aData.rect, Browser.selectedBrowser.contentDocumentHeight);
    }
    this.updateCommands();
  },

  updateCommands: function findHelperUpdateCommands() {
    let disabled = (this._status == Ci.nsITypeAheadFind.FIND_NOTFOUND) || (this._searchString == "");
    this._cmdPrevious.setAttribute("disabled", disabled);
    this._cmdNext.setAttribute("disabled", disabled);
  },

  _zoom: function _findHelperZoom(aElementRect, aContentHeight) {
    // The rect we get here is the content rect including scroll offset
    // in the page.

    // If the text falls below the find bar and keyboard shift content up.
    let browserShift = 0;
    // aElementRect.y is the top left origin of the selection rect.
    if ((aElementRect.y + aElementRect.height) >
        (aContentHeight - this._container.boxObject.height)) {
      browserShift += this._container.boxObject.height;
    }
    browserShift += Services.metro.keyboardHeight;

    // If the rect top of the selection is above the view, don't shift content
    // (or if it's already shifted, shift it back down).
    if (aElementRect.y < browserShift) {
      browserShift = 0;
    }

    // Shift the deck so that the selection is within the visible view.
    ContentAreaObserver.shiftBrowserDeck(browserShift);

    // Adjust for keyboad display and position the text selection rect in
    // the middle of the viewable area.
    let xPos = aElementRect.x;
    let yPos = aElementRect.y;
    let scrollAdjust = ((ContentAreaObserver.height - Services.metro.keyboardHeight) * .5) +
      Services.metro.keyboardHeight;
    yPos -= scrollAdjust;
    if (yPos < 0) {
      yPos = 0;
    }

    // TODO zoom via apzc, right now all we support is scroll
    // positioning.
    Browser.selectedBrowser.contentWindow.scrollTo(xPos, yPos);
  }
};
