/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabsPopup"];

class TabsPopup {
  /*
   * Handle menuitem rows for tab objects in a menupopup.
   *
   * @param {object} opts Options for configuring this instance.
   * @param {string} opts.className
   *                 An optional class name to be added to menuitem elements.
   * @param {function} opts.filterFn
   *                   A function to filter which tabs are added to the popup.
   * @param {object} opts.insertBefore
   *                 An optional element to insert the menuitems before in the
   *                 popup, if omitted they will be appended to popup.
   * @param {function} opts.onPopulate
   *                   An optional function that will be called with the
   *                   popupshowing event that caused the menu to be populated.
   * @param {object} opts.popup
   *                 A menupopup element to populate and register the show/hide
   *                 listeners on.
   */
  constructor({className, filterFn, insertBefore, onPopulate, popup}) {
    this.className = className;
    this.filterFn = filterFn;
    this.insertBefore = insertBefore;
    this.onPopulate = onPopulate;
    this.popup = popup;

    this.doc = popup.ownerDocument;
    this.gBrowser = this.doc.defaultView.gBrowser;
    this.tabToMenuitem = new Map();
    this.popup.addEventListener("popupshowing", this);
  }

  handleEvent(event) {
    switch (event.type) {
      case "TabAttrModified":
        this._tabAttrModified(event.target);
        break;
      case "TabClose":
        this._tabClose(event.target);
        break;
      case "command":
        this._handleCommand(event.target.tab);
        break;
      case "popuphidden":
        if (event.target == this.popup) {
          this._cleanup();
        }
        break;
      case "popupshowing":
        if (event.target == this.popup) {
          this._populate();
          if (typeof this.onPopulate == "function") {
            this.onPopulate(event);
          }
        }
        break;
    }
  }

  /*
   * Populate the popup with menuitems and setup the listeners.
   */
  _populate() {
    let fragment = this.doc.createDocumentFragment();

    for (let tab of this.gBrowser.tabs) {
      if (this.filterFn(tab)) {
        fragment.appendChild(this._createMenuitem(tab));
      }
    }

    if (this.insertBefore) {
      this.popup.insertBefore(fragment, this.insertBefore);
    } else {
      this.popup.appendChild(fragment);
    }

    this._setupListeners();
  }

  /*
   * Remove the menuitems from the DOM, cleanup internal state and listeners.
   */
  _cleanup() {
    for (let item of this.tabToMenuitem.values()) {
      item.remove();
    }
    this.tabToMenuitem = new Map();
    this._cleanupListeners();
  }

  _setupListeners() {
    this.gBrowser.tabContainer.addEventListener("TabAttrModified", this);
    this.gBrowser.tabContainer.addEventListener("TabClose", this);
    this.popup.addEventListener("popuphidden", this);
  }

  _cleanupListeners() {
    this.gBrowser.tabContainer.removeEventListener("TabAttrModified", this);
    this.gBrowser.tabContainer.removeEventListener("TabClose", this);
    this.popup.removeEventListener("popuphidden", this);
  }

  _tabAttrModified(tab) {
    let item = this.tabToMenuitem.get(tab);
    if (item) {
      if (!this.filterFn(tab)) {
        // If the tab is no longer in this set of tabs, hide the item.
        this._removeItem(item, tab);
      } else {
        this._setMenuitemAttributes(item, tab);
      }
    }
  }

  _tabClose(tab) {
    let item = this.tabToMenuitem.get(tab);
    if (item) {
      this._removeItem(item, tab);
    }
  }

  _removeItem(item, tab) {
    this.tabToMenuitem.delete(tab);
    item.remove();
  }

  _handleCommand(tab) {
    if (this.gBrowser.selectedTab != tab) {
      this.gBrowser.selectedTab = tab;
    } else {
      this.gBrowser.tabContainer._handleTabSelect();
    }
  }

  _createMenuitem(tab) {
    let item = this.doc.createElementNS(
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
      "menuitem");
    item.tab = tab;

    item.setAttribute("class", "menuitem-iconic menuitem-with-favicon");
    if (this.className) {
      item.classList.add(this.className);
    }
    this._setMenuitemAttributes(item, tab);

    this.tabToMenuitem.set(tab, item);

    item.addEventListener("command", this);

    return item;
  }

  _setMenuitemAttributes(item, tab) {
    item.setAttribute("label", tab.label);
    item.setAttribute("crop", "end");

    if (tab.hasAttribute("busy")) {
      item.setAttribute("busy", tab.getAttribute("busy"));
      item.removeAttribute("iconloadingprincipal");
      item.removeAttribute("image");
    } else {
      item.setAttribute("iconloadingprincipal", tab.getAttribute("iconloadingprincipal"));
      item.setAttribute("image", tab.getAttribute("image"));
      item.removeAttribute("busy");
    }

    if (tab.hasAttribute("pending"))
      item.setAttribute("pending", tab.getAttribute("pending"));
    else
      item.removeAttribute("pending");

    if (tab.selected)
      item.setAttribute("selected", "true");
    else
      item.removeAttribute("selected");

    let addEndImage = () => {
      let endImage = this.doc.createElement("image");
      endImage.setAttribute("class", "alltabs-endimage");
      let endImageContainer = this.doc.createElement("hbox");
      endImageContainer.setAttribute("align", "center");
      endImageContainer.setAttribute("pack", "center");
      endImageContainer.appendChild(endImage);
      item.appendChild(endImageContainer);
      return endImage;
    };

    if (item.firstChild)
      item.firstChild.remove();
    if (tab.hasAttribute("muted"))
      addEndImage().setAttribute("muted", "true");
    else if (tab.hasAttribute("soundplaying"))
      addEndImage().setAttribute("soundplaying", "true");
  }
}
