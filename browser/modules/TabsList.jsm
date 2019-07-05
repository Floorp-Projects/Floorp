/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "PanelMultiView",
  "resource:///modules/PanelMultiView.jsm"
);

var EXPORTED_SYMBOLS = ["TabsPanel"];

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
  constructor({ className, filterFn, insertBefore, containerNode }) {
    this.className = className;
    this.filterFn = filterFn;
    this.insertBefore = insertBefore;
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
        fragment.appendChild(this._createRow(tab));
      }
    }

    this._addElement(fragment);
    this._setupListeners();
  }

  _addElement(elementOrFragment) {
    this.containerNode.insertBefore(elementOrFragment, this.insertBefore);
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
        // The tab no longer matches our criteria, remove it.
        this._removeItem(item, tab);
      } else {
        this._setRowAttributes(item, tab);
      }
    } else if (this.filterFn(tab)) {
      // The tab now matches our criteria, add a row for it.
      this._addTab(tab);
    }
  }

  _addTab(newTab) {
    let newRow = this._createRow(newTab);
    let nextTab = newTab.nextElementSibling;

    while (nextTab && !this.filterFn(nextTab)) {
      nextTab = nextTab.nextElementSibling;
    }

    if (nextTab) {
      // If we found a tab after this one in the list, insert the new row before it.
      let nextRow = this.tabToElement.get(nextTab);
      nextRow.parentNode.insertBefore(newRow, nextRow);
    } else {
      // If there's no next tab then insert it as usual.
      this._addElement(newRow);
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

const TABS_PANEL_EVENTS = {
  show: "ViewShowing",
  hide: "PanelMultiViewHidden",
};

class TabsPanel extends TabsListBase {
  constructor(opts) {
    super({
      ...opts,
      containerNode: opts.containerNode || opts.view.firstElementChild,
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

  _populate(event) {
    super._populate(event);

    // The loading throbber can't be set until the toolbarbutton is rendered,
    // so set the image attributes again now that the elements are in the DOM.
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
    let { doc } = this;
    let row = doc.createXULElement("toolbaritem");
    row.setAttribute("class", "all-tabs-item");
    if (this.className) {
      row.classList.add(this.className);
    }
    row.tab = tab;
    row.addEventListener("command", this);
    this.tabToElement.set(tab, row);

    let button = doc.createXULElement("toolbarbutton");
    button.setAttribute(
      "class",
      "all-tabs-button subviewbutton subviewbutton-iconic"
    );
    button.setAttribute("flex", "1");
    button.setAttribute("crop", "right");
    button.tab = tab;

    row.appendChild(button);

    let secondaryButton = doc.createXULElement("toolbarbutton");
    secondaryButton.setAttribute(
      "class",
      "all-tabs-secondary-button subviewbutton subviewbutton-iconic"
    );
    secondaryButton.setAttribute("closemenu", "none");
    secondaryButton.setAttribute("toggle-mute", "true");
    secondaryButton.tab = tab;
    row.appendChild(secondaryButton);

    this._setRowAttributes(row, tab);

    return row;
  }

  _setRowAttributes(row, tab) {
    setAttributes(row, { selected: tab.selected });

    let busy = tab.getAttribute("busy");
    let button = row.firstElementChild;
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
      pictureinpicture: tab.pictureinpicture,
      hidden: !(tab.muted || tab.soundPlaying),
    });
  }

  _setImageAttributes(row, tab) {
    let button = row.firstElementChild;
    let image =
      this.doc.getAnonymousElementByAttribute(
        button,
        "class",
        "toolbarbutton-icon"
      ) ||
      this.doc.getAnonymousElementByAttribute(
        button,
        "class",
        "toolbarbutton-icon tab-throbber-tabslist"
      );

    if (image) {
      let busy = tab.getAttribute("busy");
      let progress = tab.getAttribute("progress");
      setAttributes(image, { busy, progress });
      if (busy) {
        image.classList.add("tab-throbber-tabslist");
      } else {
        image.classList.remove("tab-throbber-tabslist");
      }
    }
  }
}
