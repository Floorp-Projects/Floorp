/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;

// The XUL and XHTML namespace.
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

const HUD_STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";


Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "stringBundle", function () {
  return Services.strings.createBundle(HUD_STRINGS_URI);
});


this.EXPORTED_SYMBOLS = ["AutocompletePopup"];

/**
 * Autocomplete popup UI implementation.
 *
 * @constructor
 * @param nsIDOMDocument aDocument
 *        The document you want the popup attached to.
 */
this.AutocompletePopup = function AutocompletePopup(aDocument)
{
  this._document = aDocument;

  // Reuse the existing popup elements.
  this._panel = this._document.getElementById("webConsole_autocompletePopup");
  if (!this._panel) {
    this._panel = this._document.createElementNS(XUL_NS, "panel");
    this._panel.setAttribute("id", "webConsole_autocompletePopup");
    this._panel.setAttribute("label",
      stringBundle.GetStringFromName("Autocomplete.label"));
    this._panel.setAttribute("noautofocus", "true");
    this._panel.setAttribute("ignorekeys", "true");
    this._panel.setAttribute("level", "top");

    let mainPopupSet = this._document.getElementById("mainPopupSet");
    if (mainPopupSet) {
      mainPopupSet.appendChild(this._panel);
    }
    else {
      this._document.documentElement.appendChild(this._panel);
    }

    this._list = this._document.createElementNS(XUL_NS, "richlistbox");
    this._list.flex = 1;
    this._panel.appendChild(this._list);

    // Open and hide the panel, so we initialize the API of the richlistbox.
    this._panel.width = 1;
    this._panel.height = 1;
    this._panel.openPopup(null, "overlap", 0, 0, false, false);
    this._panel.hidePopup();
    this._panel.width = "";
    this._panel.height = "";
  }
  else {
    this._list = this._panel.firstChild;
  }
}

AutocompletePopup.prototype = {
  _document: null,
  _panel: null,
  _list: null,

  /**
   * Open the autocomplete popup panel.
   *
   * @param nsIDOMNode aAnchor
   *        Optional node to anchor the panel to.
   */
  openPopup: function AP_openPopup(aAnchor)
  {
    this._panel.openPopup(aAnchor, "after_start", 0, 0, false, false);

    if (this.onSelect) {
      this._list.addEventListener("select", this.onSelect, false);
    }

    if (this.onClick) {
      this._list.addEventListener("click", this.onClick, false);
    }

    this._updateSize();
  },

  /**
   * Hide the autocomplete popup panel.
   */
  hidePopup: function AP_hidePopup()
  {
    this._panel.hidePopup();

    if (this.onSelect) {
      this._list.removeEventListener("select", this.onSelect, false);
    }

    if (this.onClick) {
      this._list.removeEventListener("click", this.onClick, false);
    }
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

    this._document = null;
    this._list = null;
    this._panel = null;
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
      // We need the timeout to allow the content to reflow. Attempting to
      // update the richlistbox size too early does not work.
      this._document.defaultView.setTimeout(this._updateSize.bind(this), 1);
    }
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
    this._list.width = this._panel.clientWidth +
                       this._scrollbarWidth;
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

    // Reset the panel and list dimensions. New dimensions are calculated when a
    // new set of items is added to the autocomplete popup.
    this._list.width = "";
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
    if (this._list.ensureIndexIsVisible) {
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
    this._list.ensureIndexIsVisible(this._list.selectedIndex);
  },

  /**
   * Append an item into the autocomplete list.
   *
   * @param object aItem
   *        The item you want appended to the list. The object must have a
   *        "label" property which is used as the displayed value.
   */
  appendItem: function AP_appendItem(aItem)
  {
    let description = this._document.createElementNS(XUL_NS, "description");
    description.textContent = aItem.label;

    let listItem = this._document.createElementNS(XUL_NS, "richlistitem");
    listItem.appendChild(description);
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

