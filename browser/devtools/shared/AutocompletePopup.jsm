/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;

// The XUL and XHTML namespace.
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["AutocompletePopup"];

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
 *        - fixedWidth {Boolean} Boolean to control dynamic width of the popup.
 *        - direction {String} The direction of the text in the panel. rtl or ltr
 *        - onSelect {String} The select event handler for the richlistbox
 *        - onClick {String} The click event handler for the richlistbox.
 *        - onKeypress {String} The keypress event handler for the richlistitems.
 */
this.AutocompletePopup =
function AutocompletePopup(aDocument, aOptions = {})
{
  this._document = aDocument;

  this.fixedWidth = aOptions.fixedWidth || false;
  this.autoSelect = aOptions.autoSelect || false;
  this.position = aOptions.position || "after_start";
  this.direction = aOptions.direction || "ltr";

  this.onSelect = aOptions.onSelect;
  this.onClick = aOptions.onClick;
  this.onKeypress = aOptions.onKeypress;

  let id = aOptions.panelId || "devtools_autoCompletePopup";
  let theme = aOptions.theme || "dark";
  // Reuse the existing popup elements.
  this._panel = this._document.getElementById(id);
  if (!this._panel) {
    this._panel = this._document.createElementNS(XUL_NS, "panel");
    this._panel.setAttribute("id", id);
    this._panel.className = "devtools-autocomplete-popup " + theme + "-theme";

    this._panel.setAttribute("noautofocus", "true");
    this._panel.setAttribute("level", "top");
    if (!aOptions.onKeypress) {
      this._panel.setAttribute("ignorekeys", "true");
    }

    let mainPopupSet = this._document.getElementById("mainPopupSet");
    if (mainPopupSet) {
      mainPopupSet.appendChild(this._panel);
    }
    else {
      this._document.documentElement.appendChild(this._panel);
    }
    this._list = null;
  }
  else {
    this._list = this._panel.firstChild;
  }

  if (!this._list) {
    this._list = this._document.createElementNS(XUL_NS, "richlistbox");
    this._panel.appendChild(this._list);

    // Open and hide the panel, so we initialize the API of the richlistbox.
    this._panel.openPopup(null, this.popup, 0, 0);
    this._panel.hidePopup();
  }

  this._list.flex = 1;
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
}

AutocompletePopup.prototype = {
  _document: null,
  _panel: null,
  _list: null,

  // Event handlers.
  onSelect: null,
  onClick: null,
  onKeypress: null,

  /**
   * Open the autocomplete popup panel.
   *
   * @param nsIDOMNode aAnchor
   *        Optional node to anchor the panel to.
   */
  openPopup: function AP_openPopup(aAnchor)
  {
    this._panel.openPopup(aAnchor, this.position, 0, 0);

    if (this.autoSelect) {
      this.selectFirstItem();
    }
    if (!this.fixedWidth) {
      this._updateSize();
    }
  },

  /**
   * Hide the autocomplete popup panel.
   */
  hidePopup: function AP_hidePopup()
  {
    this._panel.hidePopup();
  },

  /**
   * Check if the autocomplete popup is open.
   */
  get isOpen() {
    return this._panel.state == "open";
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
    this.clearItems();

    if (this.onSelect) {
      this._list.removeEventListener("select", this.onSelect, false);
    }

    if (this.onClick) {
      this._list.removeEventListener("click", this.onClick, false);
    }

    if (this.onKeypress) {
      this._list.removeEventListener("keypress", this.onKeypress, false);
    }

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
      if (!this.fixedWidth) {
        this._updateSize();
      }
    }
  },

  /**
   * Selects the first item of the richlistbox. Note that first item here is the
   * item closes to the input element, which means that 0th index if position is
   * below, and last index if position is above.
   */
  selectFirstItem: function AP_selectFirstItem()
  {
    if (this.position.contains("before")) {
      this.selectedIndex = this.itemCount - 1;
    }
    else {
      this.selectedIndex = 0;
    }
  },

  /**
   * Update the panel size to fit the content.
   *
   * @private
   */
  _updateSize: function AP__updateSize()
  {
    // We need the timeout to allow the content to reflow. Attempting to
    // update the richlistbox size too early does not work.
    this._document.defaultView.setTimeout(function() {
      if (!this._panel) {
        return;
      }
      this._list.width = this._panel.clientWidth +
                         this._scrollbarWidth;
      // Height change is required, otherwise the panel is drawn at an offset
      // the first time.
      this._list.height = this._panel.clientHeight;
      // This brings the panel back at right position.
      this._list.top = 0;
      // Changing panel height might make the selected item out of view, so
      // bring it back to view.
      this._list.ensureIndexIsVisible(this._list.selectedIndex);
    }.bind(this), 5);
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

    if (!this.fixedWidth) {
      // Reset the panel and list dimensions. New dimensions are calculated when
      // a new set of items is added to the autocomplete popup.
      this._list.width = "";
      this._list.height = "";
      this._panel.width = "";
      this._panel.height = "";
      this._panel.top = "";
      this._panel.left = "";
    }
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
      this.selectedIndex = -1;
    }

    return this.selectedItem;
  },

  /**
   * Select the previous item in the list.
   *
   * @return object
   *         The newly selected item object.
   */
  selectPreviousItem: function AP_selectPreviousItem()
  {
    if (this.selectedIndex > -1) {
      this.selectedIndex--;
    }
    else {
      this.selectedIndex = this.itemCount - 1;
    }

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
   * Determine the scrollbar width in the current document.
   *
   * @private
   */
  get _scrollbarWidth()
  {
    if (this.__scrollbarWidth) {
      return this.__scrollbarWidth;
    }

    let hbox = this._document.createElementNS(XUL_NS, "hbox");
    hbox.setAttribute("style", "height: 0%; overflow: hidden");

    let scrollbar = this._document.createElementNS(XUL_NS, "scrollbar");
    scrollbar.setAttribute("orient", "vertical");
    hbox.appendChild(scrollbar);

    this._document.documentElement.appendChild(hbox);
    this.__scrollbarWidth = scrollbar.clientWidth;
    this._document.documentElement.removeChild(hbox);

    return this.__scrollbarWidth;
  },
};

