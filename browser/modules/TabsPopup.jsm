/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabsPopup"];
const NSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

class TabsListBase {
  constructor({className, filterFn, insertBefore, onPopulate, containerNode}) {
    this.className = className;
    this.filterFn = filterFn;
    this.insertBefore = insertBefore;
    this.onPopulate = onPopulate;
    this.containerNode = containerNode;

    this.doc = containerNode.ownerDocument;
    this.gBrowser = this.doc.defaultView.gBrowser;
    this.tabToElement = new Map();
    this.listenersRegistered = false;
  }

  get rows() {
    return this.tabToElement.values();
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
        this._selectTab(event.target.tab);
        break;
    }
  }

  _selectTab(tab) {
    if (this.gBrowser.selectedTab != tab) {
      this.gBrowser.selectedTab = tab;
    } else {
      this.gBrowser.tabContainer._handleTabSelect();
    }
  }

  /*
   * Populate the popup with menuitems and setup the listeners.
   */
  _populate(event) {
    let fragment = this.doc.createDocumentFragment();

    for (let tab of this.gBrowser.tabs) {
      if (this.filterFn(tab)) {
        let row = this._createRow(tab);
        row.tab = tab;
        row.addEventListener("command", this);
        this.tabToElement.set(tab, row);
        if (this.className) {
          row.classList.add(this.className);
        }

        fragment.appendChild(row);
      }
    }

    if (this.insertBefore) {
      this.insertBefore.parentNode.insertBefore(fragment, this.insertBefore);
    } else {
      this.containerNode.appendChild(fragment);
    }

    this._setupListeners();

    if (typeof this.onPopulate == "function") {
      this.onPopulate(event);
    }
  }

  /*
   * Remove the menuitems from the DOM, cleanup internal state and listeners.
   */
  _cleanup() {
    for (let item of this.rows) {
      item.remove();
    }
    this.tabToElement = new Map();
    this._cleanupListeners();
  }

  _setupListeners() {
    this.listenersRegistered = true;
    this.gBrowser.tabContainer.addEventListener("TabAttrModified", this);
    this.gBrowser.tabContainer.addEventListener("TabClose", this);
  }

  _cleanupListeners() {
    this.gBrowser.tabContainer.removeEventListener("TabAttrModified", this);
    this.gBrowser.tabContainer.removeEventListener("TabClose", this);
    this.listenersRegistered = false;
  }

  _tabAttrModified(tab) {
    let item = this.tabToElement.get(tab);
    if (item) {
      if (!this.filterFn(tab)) {
        // If the tab is no longer in this set of tabs, hide the item.
        this._removeItem(item, tab);
      } else {
        this._setRowAttributes(item, tab);
      }
    }
  }

  _tabClose(tab) {
    let item = this.tabToElement.get(tab);
    if (item) {
      this._removeItem(item, tab);
    }
  }

  _removeItem(item, tab) {
    this.tabToElement.delete(tab);
    item.remove();
  }
}

class TabsPopup extends TabsListBase {
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
    super({className, filterFn, insertBefore, onPopulate, containerNode: popup});
    this.containerNode.addEventListener("popupshowing", this);
  }

  handleEvent(event) {
    switch (event.type) {
      case "popuphidden":
        if (event.target == this.containerNode) {
          this._cleanup();
        }
        break;
      case "popupshowing":
        if (event.target == this.containerNode) {
          this._populate(event);
        }
        break;
      default:
        super.handleEvent(event);
        break;
    }
  }

  _setupListeners() {
    super._setupListeners();
    this.containerNode.addEventListener("popuphidden", this);
  }

  _cleanupListeners() {
    super._cleanupListeners();
    this.containerNode.removeEventListener("popuphidden", this);
  }

  _createRow(tab) {
    let item = this.doc.createElementNS(NSXUL, "menuitem");
    item.setAttribute("class", "menuitem-iconic menuitem-with-favicon");
    this._setRowAttributes(item, tab);
    return item;
  }

  _setRowAttributes(item, tab) {
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
