/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

loader.lazyImporter(this, "Services", "resource://gre/modules/Services.jsm");
loader.lazyImporter(this, "gDevTools", "resource:///modules/devtools/client/framework/gDevTools.jsm");
const events  = require("devtools/shared/event-emitter");

/**
 * Autocomplete popup UI implementation.
 *
 * @constructor
 * @param nsIDOMDocument aDocument
 *        The document you want the popup attached to.
 * @param Object aOptions
 *        An object consiting any of the following options:
 *        - panelId {String} The id for the popup panel.
 *        - listBoxId {String} The id for the richlistbox inside the panel.
 *        - position {String} The position for the popup panel.
 *        - theme {String} String related to the theme of the popup.
 *        - autoSelect {Boolean} Boolean to allow the first entry of the popup
 *                     panel to be automatically selected when the popup shows.
 *        - direction {String} The direction of the text in the panel. rtl or ltr
 *        - onSelect {String} The select event handler for the richlistbox
 *        - onClick {String} The click event handler for the richlistbox.
 *        - onKeypress {String} The keypress event handler for the richlistitems.
 */
function AutocompletePopup(aDocument, aOptions = {})
{
  this._document = aDocument;

  this.autoSelect = aOptions.autoSelect || false;
  this.position = aOptions.position || "after_start";
  this.direction = aOptions.direction || "ltr";

  this.onSelect = aOptions.onSelect;
  this.onClick = aOptions.onClick;
  this.onKeypress = aOptions.onKeypress;

  let id = aOptions.panelId || "devtools_autoCompletePopup";
  let theme = aOptions.theme || "dark";
  // If theme is auto, use the devtools.theme pref
  if (theme == "auto") {
    theme = Services.prefs.getCharPref("devtools.theme");
    this.autoThemeEnabled = true;
    // Setup theme change listener.
    this._handleThemeChange = this._handleThemeChange.bind(this);
    gDevTools.on("pref-changed", this._handleThemeChange);
  }
  // Reuse the existing popup elements.
  this._panel = this._document.getElementById(id);
  if (!this._panel) {
    this._panel = this._document.createElementNS(XUL_NS, "panel");
    this._panel.setAttribute("id", id);
    this._panel.className = "devtools-autocomplete-popup devtools-monospace "
                            + theme + "-theme";

    this._panel.setAttribute("noautofocus", "true");
    this._panel.setAttribute("level", "top");
    if (!aOptions.onKeypress) {
      this._panel.setAttribute("ignorekeys", "true");
    }
    // Stop this appearing as an alert to accessibility.
    this._panel.setAttribute("role", "presentation");

    let mainPopupSet = this._document.getElementById("mainPopupSet");
    if (mainPopupSet) {
      mainPopupSet.appendChild(this._panel);
    }
    else {
      this._document.documentElement.appendChild(this._panel);
    }
  }
  else {
    this._list = this._panel.firstChild;
  }

  if (!this._list) {
    this._list = this._document.createElementNS(XUL_NS, "richlistbox");
    this._panel.appendChild(this._list);

    // Open and hide the panel, so we initialize the API of the richlistbox.
    this._panel.openPopup(null, this.position, 0, 0);
    this._panel.hidePopup();
  }

  this._list.setAttribute("flex", "1");
  this._list.setAttribute("seltype", "single");

  if (aOptions.listBoxId) {
    this._list.setAttribute("id", aOptions.listBoxId);
  }
  this._list.className = "devtools-autocomplete-listbox " + theme + "-theme";

  if (this.onSelect) {
    this._list.addEventListener("select", this.onSelect, false);
  }

  if (this.onClick) {
    this._list.addEventListener("click", this.onClick, false);
  }

  if (this.onKeypress) {
    this._list.addEventListener("keypress", this.onKeypress, false);
  }
  this._itemIdCounter = 0;

  events.decorate(this);
}
exports.AutocompletePopup = AutocompletePopup;

AutocompletePopup.prototype = {
  _document: null,
  _panel: null,
  _list: null,
  __scrollbarWidth: null,

  // Event handlers.
  onSelect: null,
  onClick: null,
  onKeypress: null,

  /**
   * Open the autocomplete popup panel.
   *
   * @param nsIDOMNode aAnchor
   *        Optional node to anchor the panel to.
   * @param Number aXOffset
   *        Horizontal offset in pixels from the left of the node to the left
   *        of the popup.
   * @param Number aYOffset
   *        Vertical offset in pixels from the top of the node to the starting
   *        of the popup.
   */
  openPopup: function AP_openPopup(aAnchor, aXOffset = 0, aYOffset = 0)
  {
    this.__maxLabelLength = -1;
    this._updateSize();
    this._panel.openPopup(aAnchor, this.position, aXOffset, aYOffset);

    if (this.autoSelect) {
      this.selectFirstItem();
    }

    this.emit("popup-opened");
  },

  /**
   * Hide the autocomplete popup panel.
   */
  hidePopup: function AP_hidePopup()
  {
    // Return accessibility focus to the input.
    this._document.activeElement.removeAttribute("aria-activedescendant");
    this._panel.hidePopup();
  },

  /**
   * Check if the autocomplete popup is open.
   */
  get isOpen() {
    return this._panel &&
           (this._panel.state == "open" || this._panel.state == "showing");
  },

  /**
   * Destroy the object instance. Please note that the panel DOM elements remain
   * in the DOM, because they might still be in use by other instances of the
   * same code. It is the responsability of the client code to perform DOM
   * cleanup.
   */
  destroy: function AP_destroy()
  {
    if (this.isOpen) {
      this.hidePopup();
    }

    if (this.onSelect) {
      this._list.removeEventListener("select", this.onSelect, false);
    }

    if (this.onClick) {
      this._list.removeEventListener("click", this.onClick, false);
    }

    if (this.onKeypress) {
      this._list.removeEventListener("keypress", this.onKeypress, false);
    }

    if (this.autoThemeEnabled) {
      gDevTools.off("pref-changed", this._handleThemeChange);
    }

    this._list.remove();
    this._panel.remove();
    this._document = null;
    this._list = null;
    this._panel = null;
  },

  /**
   * Get the autocomplete items array.
   *
   * @param Number aIndex The index of the item what is wanted.
   *
   * @return The autocomplete item at index aIndex.
   */
  getItemAtIndex: function AP_getItemAtIndex(aIndex)
  {
    return this._list.getItemAtIndex(aIndex)._autocompleteItem;
  },

  /**
   * Get the autocomplete items array.
   *
   * @return array
   *         The array of autocomplete items.
   */
  getItems: function AP_getItems()
  {
    let items = [];

    Array.forEach(this._list.childNodes, function(aItem) {
      items.push(aItem._autocompleteItem);
    });

    return items;
  },

  /**
   * Set the autocomplete items list, in one go.
   *
   * @param array aItems
   *        The list of items you want displayed in the popup list.
   */
  setItems: function AP_setItems(aItems)
  {
    this.clearItems();
    aItems.forEach(this.appendItem, this);

    // Make sure that the new content is properly fitted by the XUL richlistbox.
    if (this.isOpen) {
      if (this.autoSelect) {
        this.selectFirstItem();
      }
      this._updateSize();
    }
  },

  /**
   * Selects the first item of the richlistbox. Note that first item here is the
   * item closes to the input element, which means that 0th index if position is
   * below, and last index if position is above.
   */
  selectFirstItem: function AP_selectFirstItem()
  {
    if (this.position.includes("before")) {
      this.selectedIndex = this.itemCount - 1;
    }
    else {
      this.selectedIndex = 0;
    }
    this._list.ensureIndexIsVisible(this._list.selectedIndex);
  },

  __maxLabelLength: -1,

  get _maxLabelLength() {
    if (this.__maxLabelLength != -1) {
      return this.__maxLabelLength;
    }

    let max = 0;
    for (let i = 0; i < this._list.childNodes.length; i++) {
      let item = this._list.childNodes[i]._autocompleteItem;
      let str = item.label;
      if (item.count) {
        str += (item.count + "");
      }
      max = Math.max(str.length, max);
    }

    this.__maxLabelLength = max;
    return this.__maxLabelLength;
  },

  /**
   * Update the panel size to fit the content.
   *
   * @private
   */
  _updateSize: function AP__updateSize()
  {
    if (!this._panel) {
      return;
    }

    this._list.style.width = (this._maxLabelLength + 3) +"ch";
    this._list.ensureIndexIsVisible(this._list.selectedIndex);
  },

  /**
   * Update accessibility appropriately when the selected item is changed.
   *
   * @private
   */
  _updateAriaActiveDescendant: function AP__updateAriaActiveDescendant()
  {
    if (!this._list.selectedItem) {
      // Return accessibility focus to the input.
      this._document.activeElement.removeAttribute("aria-activedescendant");
      return;
    }
    // Focus this for accessibility so users know about the selected item.
    this._document.activeElement.setAttribute("aria-activedescendant",
                                              this._list.selectedItem.id);
  },

  /**
   * Clear all the items from the autocomplete list.
   */
  clearItems: function AP_clearItems()
  {
    // Reset the selectedIndex to -1 before clearing the list
    this.selectedIndex = -1;

    while (this._list.hasChildNodes()) {
      this._list.removeChild(this._list.firstChild);
    }

    this.__maxLabelLength = -1;

    // Reset the panel and list dimensions. New dimensions are calculated when
    // a new set of items is added to the autocomplete popup.
    this._list.width = "";
    this._list.style.width = "";
    this._list.height = "";
    this._panel.width = "";
    this._panel.height = "";
    this._panel.top = "";
    this._panel.left = "";
  },

  /**
   * Getter for the index of the selected item.
   *
   * @type number
   */
  get selectedIndex() {
    return this._list.selectedIndex;
  },

  /**
   * Setter for the selected index.
   *
   * @param number aIndex
   *        The number (index) of the item you want to select in the list.
   */
  set selectedIndex(aIndex) {
    this._list.selectedIndex = aIndex;
    if (this.isOpen && this._list.ensureIndexIsVisible) {
      this._list.ensureIndexIsVisible(this._list.selectedIndex);
    }
    this._updateAriaActiveDescendant();
  },

  /**
   * Getter for the selected item.
   * @type object
   */
  get selectedItem() {
    return this._list.selectedItem ?
           this._list.selectedItem._autocompleteItem : null;
  },

  /**
   * Setter for the selected item.
   *
   * @param object aItem
   *        The object you want selected in the list.
   */
  set selectedItem(aItem) {
    this._list.selectedItem = this._findListItem(aItem);
    if (this.isOpen) {
      this._list.ensureIndexIsVisible(this._list.selectedIndex);
    }
    this._updateAriaActiveDescendant();
  },

  /**
   * Append an item into the autocomplete list.
   *
   * @param object aItem
   *        The item you want appended to the list.
   *        The item object can have the following properties:
   *        - label {String} Property which is used as the displayed value.
   *        - preLabel {String} [Optional] The String that will be displayed
   *                   before the label indicating that this is the already
   *                   present text in the input box, and label is the text
   *                   that will be auto completed. When this property is
   *                   present, |preLabel.length| starting characters will be
   *                   removed from label.
   *        - count {Number} [Optional] The number to represent the count of
   *                autocompleted label.
   */
  appendItem: function AP_appendItem(aItem)
  {
    let listItem = this._document.createElementNS(XUL_NS, "richlistitem");
    // Items must have an id for accessibility.
    listItem.id = this._panel.id + "_item_" + this._itemIdCounter++;
    if (this.direction) {
      listItem.setAttribute("dir", this.direction);
    }
    let label = this._document.createElementNS(XUL_NS, "label");
    label.setAttribute("value", aItem.label);
    label.setAttribute("class", "autocomplete-value");
    if (aItem.preLabel) {
      let preDesc = this._document.createElementNS(XUL_NS, "label");
      preDesc.setAttribute("value", aItem.preLabel);
      preDesc.setAttribute("class", "initial-value");
      listItem.appendChild(preDesc);
      label.setAttribute("value", aItem.label.slice(aItem.preLabel.length));
    }
    listItem.appendChild(label);
    if (aItem.count && aItem.count > 1) {
      let countDesc = this._document.createElementNS(XUL_NS, "label");
      countDesc.setAttribute("value", aItem.count);
      countDesc.setAttribute("flex", "1");
      countDesc.setAttribute("class", "autocomplete-count");
      listItem.appendChild(countDesc);
    }
    listItem._autocompleteItem = aItem;

    this._list.appendChild(listItem);
  },

  /**
   * Find the richlistitem element that belongs to an item.
   *
   * @private
   *
   * @param object aItem
   *        The object you want found in the list.
   *
   * @return nsIDOMNode|null
   *         The nsIDOMNode that belongs to the given item object. This node is
   *         the richlistitem element.
   */
  _findListItem: function AP__findListItem(aItem)
  {
    for (let i = 0; i < this._list.childNodes.length; i++) {
      let child = this._list.childNodes[i];
      if (child._autocompleteItem == aItem) {
        return child;
      }
    }
    return null;
  },

  /**
   * Remove an item from the popup list.
   *
   * @param object aItem
   *        The item you want removed.
   */
  removeItem: function AP_removeItem(aItem)
  {
    let item = this._findListItem(aItem);
    if (!item) {
      throw new Error("Item not found!");
    }
    this._list.removeChild(item);
  },

  /**
   * Getter for the number of items in the popup.
   * @type number
   */
  get itemCount() {
    return this._list.childNodes.length;
  },

  /**
   * Getter for the height of each item in the list.
   *
   * @private
   *
   * @type number
   */
  get _itemHeight() {
    return this._list.selectedItem.clientHeight;
  },

  /**
   * Select the next item in the list.
   *
   * @return object
   *         The newly selected item object.
   */
  selectNextItem: function AP_selectNextItem()
  {
    if (this.selectedIndex < (this.itemCount - 1)) {
      this.selectedIndex++;
    }
    else {
      this.selectedIndex = 0;
    }

    return this.selectedItem;
  },

  /**
   * Select the previous item in the list.
   *
   * @return object
   *         The newly-selected item object.
   */
  selectPreviousItem: function AP_selectPreviousItem()
  {
    if (this.selectedIndex > 0) {
      this.selectedIndex--;
    }
    else {
      this.selectedIndex = this.itemCount - 1;
    }

    return this.selectedItem;
  },

  /**
   * Select the top-most item in the next page of items or
   * the last item in the list.
   *
   * @return object
   *         The newly-selected item object.
   */
  selectNextPageItem: function AP_selectNextPageItem()
  {
    let itemsPerPane = Math.floor(this._list.scrollHeight / this._itemHeight);
    let nextPageIndex = this.selectedIndex + itemsPerPane + 1;
    this.selectedIndex = nextPageIndex > this.itemCount - 1 ?
      this.itemCount - 1 : nextPageIndex;

    return this.selectedItem;
  },

  /**
   * Select the bottom-most item in the previous page of items,
   * or the first item in the list.
   *
   * @return object
   *         The newly-selected item object.
   */
  selectPreviousPageItem: function AP_selectPreviousPageItem()
  {
    let itemsPerPane = Math.floor(this._list.scrollHeight / this._itemHeight);
    let prevPageIndex = this.selectedIndex - itemsPerPane - 1;
    this.selectedIndex = prevPageIndex < 0 ? 0 : prevPageIndex;

    return this.selectedItem;
  },

  /**
   * Focuses the richlistbox.
   */
  focus: function AP_focus()
  {
    this._list.focus();
  },

  /**
   * Manages theme switching for the popup based on the devtools.theme pref.
   *
   * @private
   *
   * @param String aEvent
   *        The name of the event. In this case, "pref-changed".
   * @param Object aData
   *        An object passed by the emitter of the event. In this case, the
   *        object consists of three properties:
   *        - pref {String} The name of the preference that was modified.
   *        - newValue {Object} The new value of the preference.
   *        - oldValue {Object} The old value of the preference.
   */
  _handleThemeChange: function AP__handleThemeChange(aEvent, aData)
  {
    if (aData.pref == "devtools.theme") {
      this._panel.classList.toggle(aData.oldValue + "-theme", false);
      this._panel.classList.toggle(aData.newValue + "-theme", true);
      this._list.classList.toggle(aData.oldValue + "-theme", false);
      this._list.classList.toggle(aData.newValue + "-theme", true);
    }
  },
};
