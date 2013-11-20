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

  _open: false,
  _status: null,

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

    // Listen for find assistant messages from content
    messageManager.addMessageListener("FindAssist:Show", this);
    messageManager.addMessageListener("FindAssist:Hide", this);

    // Listen for events where form assistant should be closed
    Elements.tabList.addEventListener("TabSelect", this, true);
    Elements.browsers.addEventListener("URLChanged", this, true);
    window.addEventListener("MozAppbarShowing", this);
  },

  receiveMessage: function findHelperReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch(aMessage.name) {
      case "FindAssist:Show":
        ContextUI.dismiss();
        this.status = json.result;
        // null rect implies nothing found
        if (json.rect) {
          this._zoom(Rect.fromRect(json.rect), json.contentHeight);
        }
        break;

      case "FindAssist:Hide":
        if (this._container.getAttribute("type") == this.type)
          this.hide();
        break;
    }
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
          if (aEvent.shiftKey) {
            this.goToPrevious();
          } else {
            this.goToNext();
          }
        }
        break;

      case "MozAppbarShowing":
        if (aEvent.target != this._container) {
          this.hide();
        }
        break;
    }
  },

  show: function findHelperShow() {
    if (BrowserUI.isStartTabVisible || this._open)
      return;

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

      // Restore the scroll synchronisation
      Browser.selectedBrowser.scrollSync = true;
    };

    this._textbox.blur();
    this._container.addEventListener("transitionend", onTransitionEnd, true);
    this._container.dismiss();
  },

  goToPrevious: function findHelperGoToPrevious() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Previous", { });
  },

  goToNext: function findHelperGoToNext() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Next", { });
  },

  search: function findHelperSearch(aValue) {
    this.updateCommands(aValue);

    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Find", { searchString: aValue });
  },

  updateCommands: function findHelperUpdateCommands(aValue) {
    let disabled = (this._status == Ci.nsITypeAheadFind.FIND_NOTFOUND) || (aValue == "");
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
