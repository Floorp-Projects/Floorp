/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(
  this,
  "HTMLTooltip",
  "devtools/client/shared/widgets/tooltip/HTMLTooltip",
  true
);
loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);

const HTML_NS = "http://www.w3.org/1999/xhtml";
let itemIdCounter = 0;

/**
 * Autocomplete popup UI implementation.
 *
 * @constructor
 * @param {Document} toolboxDoc
 *        The toolbox document to attach the autocomplete popup panel.
 * @param {Object} options
 *        An object consiting any of the following options:
 *        - listId {String} The id for the list <UL> element.
 *        - position {String} The position for the tooltip ("top" or "bottom").
 *        - useXulWrapper {Boolean} If the tooltip is hosted in a XUL document, use a
 *          XUL panel in order to use all the screen viewport available (defaults to false).
 *        - autoSelect {Boolean} Boolean to allow the first entry of the popup
 *          panel to be automatically selected when the popup shows.
 *        - onSelect {String} Callback called when the selected index is updated.
 *        - onClick {String} Callback called when the autocomplete popup receives a click
 *          event. The selectedIndex will already be updated if need be.
 *        - input {Element} Optional input element the popup will be bound to. If provided
 *          the event listeners for navigating the autocomplete list are going to be
 *          automatically added.
 */
function AutocompletePopup(toolboxDoc, options = {}) {
  EventEmitter.decorate(this);

  this._document = toolboxDoc;
  this.autoSelect = options.autoSelect || false;
  this.listId = options.listId || null;
  this.position = options.position || "bottom";
  this.useXulWrapper = options.useXulWrapper || false;

  this.onSelectCallback = options.onSelect;
  this.onClickCallback = options.onClick;

  // Array of raw autocomplete items
  this.items = [];
  // Map of autocompleteItem to HTMLElement
  this.elements = new WeakMap();

  this.selectedIndex = -1;

  this.onClick = this.onClick.bind(this);
  this.onInputKeyDown = this.onInputKeyDown.bind(this);
  this.onInputBlur = this.onInputBlur.bind(this);

  if (options.input) {
    this.input = options.input;
    options.input.addEventListener("keydown", this.onInputKeyDown);
    options.input.addEventListener("blur", this.onInputBlur);
  }
}

AutocompletePopup.prototype = {
  _document: null,

  get list() {
    if (this._list) {
      return this._list;
    }

    this._list = this._document.createElementNS(HTML_NS, "ul");
    this._list.setAttribute("flex", "1");

    // The list clone will be inserted in the same document as the anchor, and will be a
    // copy of the main list to allow screen readers to access the list.
    this._listClone = this._list.cloneNode();
    this._listClone.className = "devtools-autocomplete-list-aria-clone";

    if (this.listId) {
      this._list.setAttribute("id", this.listId);
    }

    this._list.className = "devtools-autocomplete-listbox";

    // We need to retrieve the item padding in order to correct the offset of the popup.
    const paddingPropertyName = "--autocomplete-item-padding-inline";
    const listPadding = this._document.defaultView
      .getComputedStyle(this._list)
      .getPropertyValue(paddingPropertyName)
      .replace("px", "");

    this._listPadding = 0;
    if (!Number.isNaN(Number(listPadding))) {
      this._listPadding = Number(listPadding);
    }

    this._list.addEventListener("click", this.onClick);

    return this._list;
  },

  get tooltip() {
    if (this._tooltip) {
      return this._tooltip;
    }

    this._tooltip = new HTMLTooltip(this._document, {
      useXulWrapper: this.useXulWrapper,
    });

    this._tooltip.panel.classList.add(
      "devtools-autocomplete-popup",
      "devtools-monospace"
    );
    this._tooltip.panel.appendChild(this.list);
    this._tooltip.setContentSize({ height: "auto" });

    return this._tooltip;
  },

  onInputKeyDown(event) {
    // Only handle the even if the popup is opened.
    if (!this.isOpen) {
      return;
    }

    if (
      this.selectedItem &&
      this.onClickCallback &&
      (event.key === "Enter" ||
        (event.key === "ArrowRight" && !event.shiftKey) ||
        (event.key === "Tab" && !event.shiftKey))
    ) {
      this.onClickCallback(event, this.selectedItem);

      // Prevent the associated keypress to be triggered.
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    // Close the popup when the user hit Left Arrow, but let the keypress be triggered
    // so the cursor moves as the user wanted.
    if (event.key === "ArrowLeft" && !event.shiftKey) {
      this.clearItems();
      this.hidePopup();
      return;
    }

    // Close the popup when the user hit Escape.
    if (event.key === "Escape") {
      this.clearItems();
      this.hidePopup();
      // Prevent the associated keypress to be triggered.
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    if (event.key === "ArrowDown") {
      this.selectNextItem();
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    if (event.key === "ArrowUp") {
      this.selectPreviousItem();
      event.preventDefault();
      event.stopPropagation();
    }
  },

  onInputBlur(event) {
    if (this.isOpen) {
      this.clearItems();
      this.hidePopup();
    }
  },

  onSelect(e) {
    if (this.onSelectCallback) {
      this.onSelectCallback(e);
    }
  },

  onClick(e) {
    const itemEl = e.target.closest(".autocomplete-item");
    const index =
      typeof itemEl?.dataset?.index !== "undefined"
        ? parseInt(itemEl.dataset.index, 10)
        : null;

    if (index !== null) {
      this.selectItemAtIndex(index);
    }

    this.emit("popup-click");

    if (this.onClickCallback) {
      const item = index !== null ? this.items[index] : null;
      this.onClickCallback(e, item);
    }
  },

  /**
   * Open the autocomplete popup panel.
   *
   * @param {Node} anchor
   *        Optional node to anchor the panel to. Will default to this.input if it exists.
   * @param {Number} xOffset
   *        Horizontal offset in pixels from the left of the node to the left
   *        of the popup.
   * @param {Number} yOffset
   *        Vertical offset in pixels from the top of the node to the starting
   *        of the popup.
   * @param {Number} index
   *        The position of item to select.
   * @param {Object} options: Check `selectItemAtIndex` for more information.
   */
  async openPopup(anchor, xOffset = 0, yOffset = 0, index, options) {
    if (!anchor && this.input) {
      anchor = this.input;
    }

    // Retrieve the anchor's document active element to add accessibility metadata.
    this._activeElement = anchor.ownerDocument.activeElement;

    // We want the autocomplete items to be perflectly lined-up with the string the
    // user entered, so we need to remove the left-padding and the left-border from
    // the xOffset.
    const leftBorderSize = 1;

    // If we have another call to openPopup while the previous one isn't over yet, we
    // need to wait until it's settled to not be in a compromised state.
    if (this._pendingShowPromise) {
      await this._pendingShowPromise;
    }

    this._pendingShowPromise = this.tooltip.show(anchor, {
      x: xOffset - this._listPadding - leftBorderSize,
      y: yOffset,
      position: this.position,
    });
    await this._pendingShowPromise;
    this._pendingShowPromise = null;

    if (this.autoSelect) {
      this.selectItemAtIndex(index, options);
    }

    this.emit("popup-opened");
  },

  /**
   * Select item at the provided index.
   *
   * @param {Number} index
   *        The position of the item to select.
   * @param {Object} options: An object that can contain:
   *        -  {Boolean} preventSelectCallback: true to not call this.onSelectCallback as
   *                     during the initial autoSelect.
   */
  selectItemAtIndex(index, options = {}) {
    const { preventSelectCallback } = options;

    if (!Number.isInteger(index)) {
      // If no index was provided, select the first item.
      index = 0;
    }
    const item = this.items[index];
    const element = this.elements.get(item);

    const previousSelected = this.list.querySelector(".autocomplete-selected");
    if (previousSelected && previousSelected !== element) {
      previousSelected.classList.remove("autocomplete-selected");
    }

    if (element && !element.classList.contains("autocomplete-selected")) {
      element.classList.add("autocomplete-selected");
    }

    if (this.isOpen && item) {
      this._scrollElementIntoViewIfNeeded(element);
      this._setActiveDescendant(element.id);
    } else {
      this._clearActiveDescendant();
    }
    this.selectedIndex = index;

    if (
      this.isOpen &&
      item &&
      this.onSelectCallback &&
      !preventSelectCallback
    ) {
      // Call the user-defined select callback if defined.
      this.onSelectCallback(item);
    }
  },

  /**
   * Hide the autocomplete popup panel.
   */
  hidePopup() {
    this._pendingShowPromise = null;
    this.tooltip.once("hidden", () => {
      this.emit("popup-closed");
    });

    this._clearActiveDescendant();
    this._activeElement = null;
    this.tooltip.hide();
  },

  /**
   * Check if the autocomplete popup is open.
   */
  get isOpen() {
    return !!this._tooltip && this.tooltip.isVisible();
  },

  /**
   * Destroy the object instance. Please note that the panel DOM elements remain
   * in the DOM, because they might still be in use by other instances of the
   * same code. It is the responsability of the client code to perform DOM
   * cleanup.
   */
  destroy() {
    this._pendingShowPromise = null;
    if (this.isOpen) {
      this.hidePopup();
    }

    if (this._list) {
      this._list.removeEventListener("click", this.onClick);

      this._list.remove();
      this._listClone.remove();

      this._list = null;
    }

    if (this._tooltip) {
      this._tooltip.destroy();
      this._tooltip = null;
    }

    if (this.input) {
      this.input.addEventListener("keydown", this.onInputKeyDown);
      this.input.addEventListener("blur", this.onInputBlur);
      this.input = null;
    }

    this._document = null;
  },

  /**
   * Get the autocomplete items array.
   *
   * @param {Number} index
   *        The index of the item what is wanted.
   *
   * @return {Object} The autocomplete item at index index.
   */
  getItemAtIndex(index) {
    return this.items[index];
  },

  /**
   * Get the autocomplete items array.
   *
   * @return {Array} The array of autocomplete items.
   */
  getItems() {
    // Return a copy of the array to avoid side effects from the caller code.
    return this.items.slice(0);
  },

  /**
   * Set the autocomplete items list, in one go.
   *
   * @param {Array} items
   *        The list of items you want displayed in the popup list.
   * @param {Number} selectedIndex
   *        The position of the item to select.
   * @param {Object} options: An object that can contain:
   *        -  {Boolean} preventSelectCallback: true to not call this.onSelectCallback as
   *                     during the initial autoSelect.
   */
  setItems(items, selectedIndex, options) {
    this.clearItems();

    // If there is no new items, no need to do unecessary work.
    if (items.length === 0) {
      return;
    }

    if (!Number.isInteger(selectedIndex) && this.autoSelect) {
      selectedIndex = 0;
    }

    // Let's compute the max label length in the item list. This length will then be used
    // to set the width of the popup.
    let maxLabelLength = 0;

    const fragment = this._document.createDocumentFragment();
    items.forEach((item, i) => {
      const selected = selectedIndex === i;
      const listItem = this.createListItem(item, i, selected);
      this.items.push(item);
      this.elements.set(item, listItem);
      fragment.appendChild(listItem);

      let { label, postLabel, count } = item;
      if (count) {
        label += count + "";
      }

      if (postLabel) {
        label += postLabel;
      }
      maxLabelLength = Math.max(label.length, maxLabelLength);
    });

    const fragmentClone = fragment.cloneNode(true);
    this.list.style.width = maxLabelLength + 3 + "ch";
    this.list.appendChild(fragment);
    // Update the clone content to match the current list content.
    this._listClone.appendChild(fragmentClone);

    this.selectItemAtIndex(selectedIndex, options);
  },

  _scrollElementIntoViewIfNeeded(element) {
    const quads = element.getBoxQuads({
      relativeTo: this.tooltip.panel,
      createFramesForSuppressedWhitespace: false,
    });
    if (!quads || !quads[0]) {
      return;
    }

    const { top, height } = quads[0].getBounds();
    const containerHeight = this.tooltip.panel.getBoundingClientRect().height;
    if (top < 0) {
      // Element is above container.
      element.scrollIntoView(true);
    } else if (top + height > containerHeight) {
      // Element is below container.
      element.scrollIntoView(false);
    }
  },

  /**
   * Clear all the items from the autocomplete list.
   */
  clearItems() {
    if (this._list) {
      this._list.innerHTML = "";
    }
    if (this._listClone) {
      this._listClone.innerHTML = "";
    }

    this.items = [];
    this.elements = new WeakMap();
    this.selectItemAtIndex(-1);
  },

  /**
   * Getter for the selected item.
   * @type Object
   */
  get selectedItem() {
    return this.items[this.selectedIndex];
  },

  /**
   * Setter for the selected item.
   *
   * @param {Object} item
   *        The object you want selected in the list.
   */
  set selectedItem(item) {
    const index = this.items.indexOf(item);
    if (index !== -1 && this.isOpen) {
      this.selectItemAtIndex(index);
    }
  },

  /**
   * Update the aria-activedescendant attribute on the current active element for
   * accessibility.
   *
   * @param {String} id
   *        The id (as in DOM id) of the currently selected autocomplete suggestion
   */
  _setActiveDescendant(id) {
    if (!this._activeElement) {
      return;
    }

    // Make sure the list clone is in the same document as the anchor.
    const anchorDoc = this._activeElement.ownerDocument;
    if (
      !this._listClone.parentNode ||
      this._listClone.ownerDocument !== anchorDoc
    ) {
      anchorDoc.documentElement.appendChild(this._listClone);
    }

    this._activeElement.setAttribute("aria-activedescendant", id);
  },

  /**
   * Clear the aria-activedescendant attribute on the current active element.
   */
  _clearActiveDescendant() {
    if (!this._activeElement) {
      return;
    }

    this._activeElement.removeAttribute("aria-activedescendant");
  },

  createListItem(item, index, selected) {
    const listItem = this._document.createElementNS(HTML_NS, "li");
    // Items must have an id for accessibility.
    listItem.setAttribute("id", "autocomplete-item-" + itemIdCounter++);
    listItem.classList.add("autocomplete-item");
    if (selected) {
      listItem.classList.add("autocomplete-selected");
    }
    listItem.setAttribute("data-index", index);

    if (this.direction) {
      listItem.setAttribute("dir", this.direction);
    }

    const label = this._document.createElementNS(HTML_NS, "span");
    label.textContent = item.label;
    label.className = "autocomplete-value";

    if (item.preLabel) {
      const preDesc = this._document.createElementNS(HTML_NS, "span");
      preDesc.textContent = item.preLabel;
      preDesc.className = "initial-value";
      listItem.appendChild(preDesc);
      label.textContent = item.label.slice(item.preLabel.length);
    }

    listItem.appendChild(label);

    if (item.postLabel) {
      const postDesc = this._document.createElementNS(HTML_NS, "span");
      postDesc.className = "autocomplete-postlabel";
      postDesc.textContent = item.postLabel;
      // Determines if the postlabel is a valid colour or other value
      if (this._isValidColor(item.postLabel)) {
        const colorSwatch = this._document.createElementNS(HTML_NS, "span");
        colorSwatch.className = "autocomplete-swatch autocomplete-colorswatch";
        colorSwatch.style.cssText = "background-color: " + item.postLabel;
        postDesc.insertBefore(colorSwatch, postDesc.childNodes[0]);
      }
      listItem.appendChild(postDesc);
    }

    if (item.count && item.count > 1) {
      const countDesc = this._document.createElementNS(HTML_NS, "span");
      countDesc.textContent = item.count;
      countDesc.setAttribute("flex", "1");
      countDesc.className = "autocomplete-count";
      listItem.appendChild(countDesc);
    }

    return listItem;
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
      const listHeight = this.tooltip.panel.clientHeight;
      const element = this.elements.get(this.items[0]);
      const elementHeight = element.getBoundingClientRect().height;
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
  selectNextItem() {
    if (this.selectedIndex < this.items.length - 1) {
      this.selectItemAtIndex(this.selectedIndex + 1);
    } else {
      this.selectItemAtIndex(0);
    }
    return this.selectedItem;
  },

  /**
   * Select the previous item in the list.
   *
   * @return {Object}
   *         The newly-selected item object.
   */
  selectPreviousItem() {
    if (this.selectedIndex > 0) {
      this.selectItemAtIndex(this.selectedIndex - 1);
    } else {
      this.selectItemAtIndex(this.items.length - 1);
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
  selectNextPageItem() {
    const nextPageIndex = this.selectedIndex + this._itemsPerPane + 1;
    this.selectItemAtIndex(Math.min(nextPageIndex, this.itemCount - 1));
    return this.selectedItem;
  },

  /**
   * Select the bottom-most item in the previous page of items,
   * or the first item in the list.
   *
   * @return {Object}
   *         The newly-selected item object.
   */
  selectPreviousPageItem() {
    const prevPageIndex = this.selectedIndex - this._itemsPerPane - 1;
    this.selectItemAtIndex(Math.max(prevPageIndex, 0));
    return this.selectedItem;
  },

  /**
   * Determines if the specified colour object is a valid colour, and if
   * it is not a "special value"
   *
   * @return {Boolean}
   *         If the object represents a proper colour or not.
   */
  _isValidColor(color) {
    const colorObj = new colorUtils.CssColor(color);
    return colorObj.valid && !colorObj.specialValue;
  },

  /**
   * Used by tests.
   */
  get _panel() {
    return this.tooltip.panel;
  },

  /**
   * Used by tests.
   */
  get _window() {
    return this._document.defaultView;
  },
};

module.exports = AutocompletePopup;
