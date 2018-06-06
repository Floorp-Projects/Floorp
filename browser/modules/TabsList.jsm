/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "PanelMultiView",
                               "resource:///modules/PanelMultiView.jsm");

var EXPORTED_SYMBOLS = ["TabsPanel", "TabsPopup"];
const NSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function setAttributes(element, attrs) {
  for (let [name, value] of Object.entries(attrs)) {
    if (value) {
      element.setAttribute(name, value);
    } else {
      element.removeAttribute(name);
    }
  }
}

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
   * Generate toolbarbuttons for tabs that meet some criteria.
   *
   * @param {object} opts Options for configuring this instance.
   * @param {function} opts.filterFn
   *                   A function to filter which tabs are used.
   * @param {object} opts.view
   *                 A panel view to listen to events on.
   * @param {object} opts.containerNode
   *                 An optional element to append elements to, if ommitted they
   *                 will be appended to opts.view.firstChild or before
   *                 opts.insertBefore.
   * @param {object} opts.insertBefore
   *                 An optional element to insert the results before, if
   *                 omitted they will be appended to opts.containerNode.
   * @param {function} opts.onPopulate
   *                   An optional function that will be called with the
   *                   popupshowing event that caused the menu to be populated.
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

const TABS_PANEL_EVENTS = {
  show: "ViewShowing",
  hide: "PanelMultiViewHidden",
};

class TabsPanel extends TabsListBase {
  constructor(opts) {
    super({
      ...opts,
      containerNode: opts.containerNode || opts.view.firstChild,
      onPopulate: (...args) => {
        this._onPopulate(...args);
        if (typeof opts.onPopulate == "function") {
          opts.onPopulate.call(this, ...args);
        }
      },
    });
    this.view = opts.view;
    this.view.addEventListener(TABS_PANEL_EVENTS.show, this);
    this.panelMultiView = null;
  }

  handleEvent(event) {
    switch (event.type) {
      case TABS_PANEL_EVENTS.hide:
        if (event.target == this.panelMultiView) {
          this._cleanup();
          this.panelMultiView = null;
        }
        break;
      case TABS_PANEL_EVENTS.show:
        if (!this.listenersRegistered && event.target == this.view) {
          this.panelMultiView = this.view.panelMultiView;
          this._populate(event);
        }
        break;
      case "command":
        if (event.target.hasAttribute("toggle-mute")) {
          event.target.tab.toggleMuteAudio();
          break;
        }
      default:
        super.handleEvent(event);
        break;
    }
  }

  _onPopulate(event) {
    // The loading throbber can't be set until the toolbarbutton is rendered,
    // so do that on first populate.
    for (let row of this.rows) {
      this._setImageAttributes(row, row.tab);
    }
  }

  _selectTab(tab) {
    super._selectTab(tab);
    PanelMultiView.hidePopup(this.view.closest("panel"));
  }

  _setupListeners() {
    super._setupListeners();
    this.panelMultiView.addEventListener(TABS_PANEL_EVENTS.hide, this);
  }

  _cleanupListeners() {
    super._cleanupListeners();
    this.panelMultiView.removeEventListener(TABS_PANEL_EVENTS.hide, this);
  }

  _createRow(tab) {
    let {doc} = this;
    let row = doc.createElementNS(NSXUL, "toolbaritem");
    row.setAttribute("class", "all-tabs-item");

    let button = doc.createElementNS(NSXUL, "toolbarbutton");
    button.setAttribute("class", "all-tabs-button subviewbutton subviewbutton-iconic");
    button.setAttribute("flex", "1");
    button.setAttribute("crop", "right");
    button.tab = tab;

    row.appendChild(button);

    let secondaryButton = doc.createElementNS(NSXUL, "toolbarbutton");
    secondaryButton.setAttribute(
      "class", "all-tabs-secondary-button subviewbutton subviewbutton-iconic");
    secondaryButton.setAttribute("closemenu", "none");
    secondaryButton.setAttribute("toggle-mute", "true");
    secondaryButton.tab = tab;
    row.appendChild(secondaryButton);

    this._setRowAttributes(row, tab);

    return row;
  }

  _setRowAttributes(row, tab) {
    setAttributes(row, {selected: tab.selected});

    let busy = tab.getAttribute("busy");
    let button = row.firstChild;
    setAttributes(button, {
      busy,
      label: tab.label,
      image: !busy && tab.getAttribute("image"),
      iconloadingprincipal: tab.getAttribute("iconloadingprincipal"),
    });

    this._setImageAttributes(row, tab);

    let secondaryButton = row.querySelector(".all-tabs-secondary-button");
    setAttributes(secondaryButton, {
      muted: tab.muted,
      soundplaying: tab.soundPlaying,
      hidden: !(tab.muted || tab.soundPlaying),
    });
  }

  _setImageAttributes(row, tab) {
    let button = row.firstChild;
    let busy = tab.getAttribute("busy");
    let image = this.doc.getAnonymousElementByAttribute(
      button, "class", "toolbarbutton-icon") ||
      this.doc.getAnonymousElementByAttribute(
        button, "class", "toolbarbutton-icon tab-throbber-fallback");

    if (image) {
      setAttributes(image, {busy});
      if (busy) {
        image.classList.add("tab-throbber-fallback");
      } else {
        image.classList.remove("tab-throbber-fallback");
      }
    }
  }
}
