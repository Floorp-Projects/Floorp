/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const HTML_NS = "http://www.w3.org/1999/xhtml";
const Services = require("Services");
const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
const EventEmitter = require("devtools/shared/event-emitter");
const {PrefObserver} = require("devtools/client/shared/prefs");
const {colorUtils} = require("devtools/shared/css/color");

let itemIdCounter = 0;
/**
 * Autocomplete popup UI implementation.
 *
 * @constructor
 * @param {Document} toolboxDoc
 *        The toolbox document to attach the autocomplete popup panel.
 * @param {Object} options
 *        An object consiting any of the following options:
 *        - listId {String} The id for the list <LI> element.
 *        - position {String} The position for the tooltip ("top" or "bottom").
 *        - theme {String} String related to the theme of the popup
 *        - autoSelect {Boolean} Boolean to allow the first entry of the popup
 *          panel to be automatically selected when the popup shows.
 *        - onSelect {String} Callback called when the selected index is updated.
 *        - onClick {String} Callback called when the autocomplete popup receives a click
 *          event. The selectedIndex will already be updated if need be.
 */
function AutocompletePopup(toolboxDoc, options = {}) {
  EventEmitter.decorate(this);

  this._document = toolboxDoc;

  this.autoSelect = options.autoSelect || false;
  this.position = options.position || "bottom";
  let theme = options.theme || "dark";

  this.onSelectCallback = options.onSelect;
  this.onClickCallback = options.onClick;

  // If theme is auto, use the devtools.theme pref
  if (theme === "auto") {
    theme = Services.prefs.getCharPref("devtools.theme");
    this.autoThemeEnabled = true;
    // Setup theme change listener.
    this._handleThemeChange = this._handleThemeChange.bind(this);
    this._prefObserver = new PrefObserver("devtools.");
    this._prefObserver.on("devtools.theme", this._handleThemeChange);
    this._currentTheme = theme;
  }

  // Create HTMLTooltip instance
  this._tooltip = new HTMLTooltip(this._document);
  this._tooltip.panel.classList.add(
    "devtools-autocomplete-popup",
    "devtools-monospace",
    theme + "-theme");
  // Stop this appearing as an alert to accessibility.
  this._tooltip.panel.setAttribute("role", "presentation");

  this._list = this._document.createElementNS(HTML_NS, "ul");
  this._list.setAttribute("flex", "1");

  // The list clone will be inserted in the same document as the anchor, and will be a
  // copy of the main list to allow screen readers to access the list.
  this._listClone = this._list.cloneNode();
  this._listClone.className = "devtools-autocomplete-list-aria-clone";

  if (options.listId) {
    this._list.setAttribute("id", options.listId);
  }
  this._list.className = "devtools-autocomplete-listbox " + theme + "-theme";

  this._tooltip.setContent(this._list);

  this.onClick = this.onClick.bind(this);
  this._list.addEventListener("click", this.onClick);

  // Array of raw autocomplete items
  this.items = [];
  // Map of autocompleteItem to HTMLElement
  this.elements = new WeakMap();

  this.selectedIndex = -1;
}

AutocompletePopup.prototype = {
  _document: null,
  _tooltip: null,
  _list: null,

  onSelect: function(e) {
    if (this.onSelectCallback) {
      this.onSelectCallback(e);
    }
  },

  onClick: function(e) {
    let item = e.target.closest(".autocomplete-item");
    if (item && typeof item.dataset.index !== "undefined") {
      this.selectedIndex = parseInt(item.dataset.index, 10);
    }

    this.emit("popup-click");
    if (this.onClickCallback) {
      this.onClickCallback(e);
    }
  },

  /**
   * Open the autocomplete popup panel.
   *
   * @param {nsIDOMNode} anchor
   *        Optional node to anchor the panel to.
   * @param {Number} xOffset
   *        Horizontal offset in pixels from the left of the node to the left
   *        of the popup.
   * @param {Number} yOffset
   *        Vertical offset in pixels from the top of the node to the starting
   *        of the popup.
   * @param {Number} index
   *        The position of item to select.
   */
  openPopup: function(anchor, xOffset = 0, yOffset = 0, index) {
    this.__maxLabelLength = -1;
    this._updateSize();

    // Retrieve the anchor's document active element to add accessibility metadata.
    this._activeElement = anchor.ownerDocument.activeElement;

    this._tooltip.show(anchor, {
      x: xOffset,
      y: yOffset,
      position: this.position,
    });

    this._tooltip.once("shown", () => {
      if (this.autoSelect) {
        this.selectItemAtIndex(index);
      }

      this.emit("popup-opened");
    });
  },

  /**
   * Select item at the provided index.
   *
   * @param {Number} index
   *        The position of the item to select.
   */
  selectItemAtIndex: function(index) {
    if (typeof index !== "number") {
      // If no index was provided, select the item closest to the input.
      let isAboveInput = this.position === "top";
      index = isAboveInput ? this.itemCount - 1 : 0;
    }
    this.selectedIndex = index;
  },

  /**
   * Hide the autocomplete popup panel.
   */
  hidePopup: function() {
    this._tooltip.once("hidden", () => {
      this.emit("popup-closed");
    });

    this._clearActiveDescendant();
    this._activeElement = null;
    this._tooltip.hide();
  },

  /**
   * Check if the autocomplete popup is open.
   */
  get isOpen() {
    return this._tooltip && this._tooltip.isVisible();
  },

  /**
   * Destroy the object instance. Please note that the panel DOM elements remain
   * in the DOM, because they might still be in use by other instances of the
   * same code. It is the responsability of the client code to perform DOM
   * cleanup.
   */
  destroy: function() {
    if (this.isOpen) {
      this.hidePopup();
    }

    this._list.removeEventListener("click", this.onClick);

    if (this.autoThemeEnabled) {
      this._prefObserver.off("devtools.theme", this._handleThemeChange);
      this._prefObserver.destroy();
    }

    this._list.remove();
    this._listClone.remove();
    this._tooltip.destroy();
    this._document = null;
    this._list = null;
    this._tooltip = null;
  },

  /**
   * Get the autocomplete items array.
   *
   * @param {Number} index
   *        The index of the item what is wanted.
   *
   * @return {Object} The autocomplete item at index index.
   */
  getItemAtIndex: function(index) {
    return this.items[index];
  },

  /**
   * Get the autocomplete items array.
   *
   * @return {Array} The array of autocomplete items.
   */
  getItems: function() {
    // Return a copy of the array to avoid side effects from the caller code.
    return this.items.slice(0);
  },

  /**
   * Set the autocomplete items list, in one go.
   *
   * @param {Array} items
   *        The list of items you want displayed in the popup list.
   * @param {Number} index
   *        The position of the item to select.
   */
  setItems: function(items, index) {
    this.clearItems();
    items.forEach(this.appendItem, this);

    if (this.isOpen && this.autoSelect) {
      this.selectItemAtIndex(index);
    }
  },

  __maxLabelLength: -1,

  get _maxLabelLength() {
    if (this.__maxLabelLength !== -1) {
      return this.__maxLabelLength;
    }

    let max = 0;

    for (let {label, postLabel, count} of this.items) {
      if (count) {
        label += count + "";
      }

      if (postLabel) {
        label += postLabel;
      }

      const length = label.length + (postLabel ? 3 : 0);
      max = Math.max(length, max);
    }

    this.__maxLabelLength = max;
    return this.__maxLabelLength;
  },

  /**
   * Update the panel size to fit the content.
   */
  _updateSize: function() {
    if (!this._tooltip) {
      return;
    }

    this._list.style.width = (this._maxLabelLength + 3) + "ch";

    let selectedItem = this.selectedItem;
    if (selectedItem) {
      this._scrollElementIntoViewIfNeeded(this.elements.get(selectedItem));
    }
  },

  _scrollElementIntoViewIfNeeded: function(element) {
    let quads = element.getBoxQuads({relativeTo: this._tooltip.panel});
    if (!quads || !quads[0]) {
      return;
    }

    let {top, height} = quads[0].bounds;
    let containerHeight = this._tooltip.panel.getBoundingClientRect().height;
    if (top < 0) {
      // Element is above container.
      element.scrollIntoView(true);
    } else if ((top + height) > containerHeight) {
      // Element is beloew container.
      element.scrollIntoView(false);
    }
  },

  /**
   * Clear all the items from the autocomplete list.
   */
  clearItems: function() {
    // Reset the selectedIndex to -1 before clearing the list
    this.selectedIndex = -1;
    this._list.innerHTML = "";
    this.__maxLabelLength = -1;
    this.items = [];
    this.elements = new WeakMap();
  },

  /**
   * Getter for the index of the selected item.
   *
   * @type {Number}
   */
  get selectedIndex() {
    return this._selectedIndex;
  },

  /**
   * Setter for the selected index.
   *
   * @param {Number} index
   *        The number (index) of the item you want to select in the list.
   */
  set selectedIndex(index) {
    let previousSelected = this._list.querySelector(".autocomplete-selected");
    if (previousSelected) {
      previousSelected.classList.remove("autocomplete-selected");
    }

    let item = this.items[index];
    if (this.isOpen && item) {
      let element = this.elements.get(item);

      element.classList.add("autocomplete-selected");
      this._scrollElementIntoViewIfNeeded(element);
      this._setActiveDescendant(element.id);
    } else {
      this._clearActiveDescendant();
    }
    this._selectedIndex = index;

    if (this.isOpen && item && this.onSelectCallback) {
      // Call the user-defined select callback if defined.
      this.onSelectCallback();
    }
  },

  /**
   * Getter for the selected item.
   * @type Object
   */
  get selectedItem() {
    return this.items[this._selectedIndex];
  },

  /**
   * Setter for the selected item.
   *
   * @param {Object} item
   *        The object you want selected in the list.
   */
  set selectedItem(item) {
    let index = this.items.indexOf(item);
    if (index !== -1 && this.isOpen) {
      this.selectedIndex = index;
    }
  },

  /**
   * Update the aria-activedescendant attribute on the current active element for
   * accessibility.
   *
   * @param {String} id
   *        The id (as in DOM id) of the currently selected autocomplete suggestion
   */
  _setActiveDescendant: function(id) {
    if (!this._activeElement) {
      return;
    }

    // Make sure the list clone is in the same document as the anchor.
    let anchorDoc = this._activeElement.ownerDocument;
    if (!this._listClone.parentNode || this._listClone.ownerDocument !== anchorDoc) {
      anchorDoc.documentElement.appendChild(this._listClone);
    }

    // Update the clone content to match the current list content.
    const clone = this._list.cloneNode(true);
    clone.className = "devtools-autocomplete-list-aria-clone";
    this._listClone.replaceWith(clone);

    // We also need to update the reference.
    this._listClone = clone;

    this._activeElement.setAttribute("aria-activedescendant", id);
  },

  /**
   * Clear the aria-activedescendant attribute on the current active element.
   */
  _clearActiveDescendant: function() {
    if (!this._activeElement) {
      return;
    }

    this._activeElement.removeAttribute("aria-activedescendant");
  },

  /**
   * Append an item into the autocomplete list.
   *
   * @param {Object} item
   *        The item you want appended to the list.
   *        The item object can have the following properties:
   *        - label {String} Property which is used as the displayed value.
   *        - preLabel {String} [Optional] The String that will be displayed
   *                   before the label indicating that this is the already
   *                   present text in the input box, and label is the text
   *                   that will be auto completed. When this property is
   *                   present, |preLabel.length| starting characters will be
   *                   removed from label.
   *        - postLabel {String} [Optional] The string that will be displayed
   *                  after the label. Currently used to display the value of
   *                  a desired variable.
   *        - count {Number} [Optional] The number to represent the count of
   *                autocompleted label.
   */
  appendItem: function(item) {
    let listItem = this._document.createElementNS(HTML_NS, "li");
    // Items must have an id for accessibility.
    listItem.setAttribute("id", "autocomplete-item-" + itemIdCounter++);
    listItem.className = "autocomplete-item";
    listItem.setAttribute("data-index", this.items.length);

    if (this.direction) {
      listItem.setAttribute("dir", this.direction);
    }

    let label = this._document.createElementNS(HTML_NS, "span");
    label.textContent = item.label;
    label.className = "autocomplete-value";

    if (item.preLabel) {
      let preDesc = this._document.createElementNS(HTML_NS, "span");
      preDesc.textContent = item.preLabel;
      preDesc.className = "initial-value";
      listItem.appendChild(preDesc);
      label.textContent = item.label.slice(item.preLabel.length);
    }

    listItem.appendChild(label);

    if (item.postLabel) {
      let postDesc = this._document.createElementNS(HTML_NS, "span");
      postDesc.className = "autocomplete-postlabel";
      postDesc.textContent = item.postLabel;
      // Determines if the postlabel is a valid colour or other value
      if (this._isValidColor(item.postLabel)) {
        let colorSwatch = this._document.createElementNS(HTML_NS, "span");
        colorSwatch.className = "autocomplete-swatch autocomplete-colorswatch";
        colorSwatch.style.cssText = "background-color: " + item.postLabel;
        postDesc.insertBefore(colorSwatch, postDesc.childNodes[0]);
      }
      listItem.appendChild(postDesc);
    }

    if (item.count && item.count > 1) {
      let countDesc = this._document.createElementNS(HTML_NS, "span");
      countDesc.textContent = item.count;
      countDesc.setAttribute("flex", "1");
      countDesc.className = "autocomplete-count";
      listItem.appendChild(countDesc);
    }

    this._list.appendChild(listItem);
    this.items.push(item);
    this.elements.set(item, listItem);
  },

  /**
   * Remove an item from the popup list.
   *
   * @param {Object} item
   *        The item you want removed.
   */
  removeItem: function(item) {
    if (!this.items.includes(item)) {
      return;
    }

    let itemIndex = this.items.indexOf(item);
    let selectedIndex = this.selectedIndex;

    // Remove autocomplete item.
    this.items.splice(itemIndex, 1);

    // Remove corresponding DOM element from the elements WeakMap and from the DOM.
    let elementToRemove = this.elements.get(item);
    this.elements.delete(elementToRemove);
    elementToRemove.remove();

    if (itemIndex <= selectedIndex) {
      // If the removed item index was before or equal to the selected index, shift the
      // selected index by 1.
      this.selectedIndex = Math.max(0, selectedIndex - 1);
    }
  },

  /**
   * Getter for the number of items in the popup.
   * @type {Number}
   */
  get itemCount() {
    return this.items.length;
  },

  /**
   * Getter for the height of each item in the list.
   *
   * @type {Number}
   */
  get _itemsPerPane() {
    if (this.items.length) {
      let listHeight = this._tooltip.panel.clientHeight;
      let element = this.elements.get(this.items[0]);
      let elementHeight = element.getBoundingClientRect().height;
      return Math.floor(listHeight / elementHeight);
    }
    return 0;
  },

  /**
   * Select the next item in the list.
   *
   * @return {Object}
   *         The newly selected item object.
   */
  selectNextItem: function() {
    if (this.selectedIndex < (this.items.length - 1)) {
      this.selectedIndex++;
    } else {
      this.selectedIndex = 0;
    }
    return this.selectedItem;
  },

  /**
   * Select the previous item in the list.
   *
   * @return {Object}
   *         The newly-selected item object.
   */
  selectPreviousItem: function() {
    if (this.selectedIndex > 0) {
      this.selectedIndex--;
    } else {
      this.selectedIndex = this.items.length - 1;
    }

    return this.selectedItem;
  },

  /**
   * Select the top-most item in the next page of items or
   * the last item in the list.
   *
   * @return {Object}
   *         The newly-selected item object.
   */
  selectNextPageItem: function() {
    let nextPageIndex = this.selectedIndex + this._itemsPerPane + 1;
    this.selectedIndex = Math.min(nextPageIndex, this.itemCount - 1);
    return this.selectedItem;
  },

  /**
   * Select the bottom-most item in the previous page of items,
   * or the first item in the list.
   *
   * @return {Object}
   *         The newly-selected item object.
   */
  selectPreviousPageItem: function() {
    let prevPageIndex = this.selectedIndex - this._itemsPerPane - 1;
    this.selectedIndex = Math.max(prevPageIndex, 0);
    return this.selectedItem;
  },

  /**
   * Manages theme switching for the popup based on the devtools.theme pref.
   */
  _handleThemeChange: function() {
    const oldValue = this._currentTheme;
    const newValue = Services.prefs.getCharPref("devtools.theme");

    this._tooltip.panel.classList.toggle(oldValue + "-theme", false);
    this._tooltip.panel.classList.toggle(newValue + "-theme", true);
    this._list.classList.toggle(oldValue + "-theme", false);
    this._list.classList.toggle(newValue + "-theme", true);

    this._currentTheme = newValue;
  },

  /**
  * Determines if the specified colour object is a valid colour, and if
  * it is not a "special value"
  *
  * @return {Boolean}
  *         If the object represents a proper colour or not.
  */
  _isValidColor: function(color) {
    let colorObj = new colorUtils.CssColor(color);
    return (colorObj.valid && (!colorObj.specialValue));
  },

  /**
   * Used by tests.
   */
  get _panel() {
    return this._tooltip.panel;
  },

  /**
   * Used by tests.
   */
  get _window() {
    return this._document.defaultView;
  },
};

module.exports = AutocompletePopup;
