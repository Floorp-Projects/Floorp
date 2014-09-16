/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu} = require("chrome");

const EventEmitter = require("devtools/toolkit/event-emitter");
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HTML_NS = "http://www.w3.org/1999/xhtml";


// Different types of events emitted by the Various components of the TableWidget
const EVENTS = {
  TABLE_CLEARED: "table-cleared",
  COLUMN_SORTED: "column-sorted",
  COLUMN_TOGGLED: "column-toggled",
  ROW_SELECTED: "row-selected",
  ROW_UPDATED: "row-updated",
  HEADER_CONTEXT_MENU: "header-context-menu",
  ROW_CONTEXT_MENU: "row-context-menu"
};

// Maximum number of character visible in any cell in the table. This is to avoid
// making the cell take up all the space in a row.
const MAX_VISIBLE_STRING_SIZE = 100;

/**
 * A table widget with various features like resizble/toggleable columns,
 * sorting, keyboard navigation etc.
 *
 * @param {nsIDOMNode} node
 *        The container element for the table widget.
 * @param {object} options
 *        - initialColumns: map of key vs display name for initial columns of
 *                          the table. See @setupColumns for more info.
 *        - uniqueId: the column which will be the unique identifier of each
 *                    entry in the table. Default: name.
 *        - emptyText: text to display when no entries in the table to display.
 *        - highlightUpdated: true to highlight the changed/added row.
 *        - removableColumns: Whether columns are removeable. If set to false,
 *                            the context menu in the headers will not appear.
 *        - firstColumn: key of the first column that should appear.
 */
function TableWidget(node, options={}) {
  EventEmitter.decorate(this);

  this.document = node.ownerDocument;
  this.window = this.document.defaultView;
  this._parent = node;

  let {initialColumns, emptyText, uniqueId, highlightUpdated, removableColumns,
       firstColumn} = options;
  this.emptyText = emptyText || "";
  this.uniqueId = uniqueId || "name";
  this.firstColumn = firstColumn || "";
  this.highlightUpdated = highlightUpdated || false;
  this.removableColumns = removableColumns !== false;

  this.tbody = this.document.createElementNS(XUL_NS, "hbox");
  this.tbody.className = "table-widget-body theme-body";
  this.tbody.setAttribute("flex", "1");
  this.tbody.setAttribute("tabindex", "0");
  this._parent.appendChild(this.tbody);

  this.placeholder = this.document.createElementNS(XUL_NS, "label");
  this.placeholder.className = "plain table-widget-empty-text";
  this.placeholder.setAttribute("flex", "1");
  this._parent.appendChild(this.placeholder);

  this.items = new Map();
  this.columns = new Map();

  // Setup the column headers context menu to allow users to hide columns at will
  if (this.removableColumns) {
    this.onPopupCommand = this.onPopupCommand.bind(this)
    this.setupHeadersContextMenu();
  }

  if (initialColumns) {
    this.setColumns(initialColumns, uniqueId);
  } else if (this.emptyText) {
    this.setPlaceholderText(this.emptyText);
  }

  this.bindSelectedRow = (event, id) => {
    this.selectedRow = id;
  };
  this.on(EVENTS.ROW_SELECTED, this.bindSelectedRow);
};

TableWidget.prototype = {

  items: null,

  /**
   * Getter for the headers context menu popup id.
   */
  get headersContextMenu() {
    if (this.menupopup) {
      return this.menupopup.id;
    }
    return null;
  },

  /**
   * Select the row corresponding to the json object `id`
   */
  set selectedRow(id) {
    for (let column of this.columns.values()) {
      column.selectRow(id[this.uniqueId] || id);
    }
  },

  /**
   * Returns the json object corresponding to the selected row.
   */
  get selectedRow() {
    return this.items.get(this.columns.get(this.uniqueId).selectedRow);
  },

  /**
   * Selects the row at index `index`.
   */
  set selectedIndex(index) {
    for (let column of this.columns.values()) {
      column.selectRowAt(index);
    }
  },

  /**
   * Returns the index of the selected row.
   */
  get selectedIndex() {
    return this.columns.get(this.uniqueId).selectedIndex;
  },

  destroy: function() {
    this.off(EVENTS.ROW_SELECTED, this.bindSelectedRow);
    if (this.menupopup) {
      this.menupopup.remove();
    }
  },

  /**
   * Sets the text to be shown when the table is empty.
   */
  setPlaceholderText: function(text) {
    this.placeholder.setAttribute("value", text);
  },

  /**
   * Prepares the context menu for the headers of the table columns. This context
   * menu allows users to toggle various columns, only with an exception of the
   * unique columns and when only two columns are visible in the table.
   */
  setupHeadersContextMenu: function() {
    let popupset = this.document.getElementsByTagName("popupset")[0];
    if (!popupset) {
      popupset = this.document.createElementNS(XUL_NS, "popupset");
      this.document.documentElement.appendChild(popupset);
    }

    this.menupopup = this.document.createElementNS(XUL_NS, "menupopup");
    this.menupopup.id = "table-widget-column-select";
    this.menupopup.addEventListener("command", this.onPopupCommand);
    popupset.appendChild(this.menupopup);
    this.populateMenuPopup();
  },

  /**
   * Populates the header context menu with the names of the columns along with
   * displaying which columns are hidden or visible.
   */
  populateMenuPopup: function() {
    if (!this.menupopup) {
      return;
    }

    while (this.menupopup.firstChild) {
      this.menupopup.firstChild.remove();
    }

    for (let column of this.columns.values()) {
      let menuitem = this.document.createElementNS(XUL_NS, "menuitem");
      menuitem.setAttribute("label", column.header.getAttribute("value"));
      menuitem.setAttribute("data-id", column.id);
      menuitem.setAttribute("type", "checkbox");
      menuitem.setAttribute("checked", !column.wrapper.getAttribute("hidden"));
      if (column.id == this.uniqueId) {
        menuitem.setAttribute("disabled", "true");
      }
      this.menupopup.appendChild(menuitem);
    }
    let checked = this.menupopup.querySelectorAll("menuitem[checked]");
    if (checked.length == 2) {
      checked[checked.length - 1].setAttribute("disabled", "true");
    }
  },

  /**
   * Event handler for the `command` event on the column headers context menu
   */
  onPopupCommand: function(event) {
    let item = event.originalTarget;
    let checked = !!item.getAttribute("checked");
    let id = item.getAttribute("data-id");
    this.emit(EVENTS.HEADER_CONTEXT_MENU, id, checked);
    checked = this.menupopup.querySelectorAll("menuitem[checked]");
    let disabled = this.menupopup.querySelectorAll("menuitem[disabled]");
    if (checked.length == 2) {
      checked[checked.length - 1].setAttribute("disabled", "true");
    } else if (disabled.length > 1) {
      disabled[disabled.length - 1].removeAttribute("disabled");
    }
  },

  /**
   * Creates the columns in the table. Without calling this method, data cannot
   * be inserted into the table unless `initialColumns` was supplied.
   *
   * @param {object} columns
   *        A key value pair representing the columns of the table. Where the
   *        key represents the id of the column and the value is the displayed
   *        label in the header of the column.
   * @param {string} sortOn
   *        The id of the column on which the table will be initially sorted on.
   * @param {array} hiddenColumns
   *        Ids of all the columns that are hidden by default.
   */
  setColumns: function(columns, sortOn = this.sortedOn, hiddenColumns = []) {
    for (let column of this.columns.values()) {
      column.destroy();
    }

    this.columns.clear();

    if (!(sortOn in columns)) {
      sortOn = null;
    }

    if (!(this.firstColumn in columns)) {
      this.firstColumn = null;
    }

    if (this.firstColumn) {
      this.columns.set(this.firstColumn,
        new Column(this, this.firstColumn, columns[this.firstColumn]));
    }

    for (let id in columns) {
      if (!sortOn) {
        sortOn = id;
      }

      if (this.firstColumn && id == this.firstColumn) {
        continue;
      }

      this.columns.set(id, new Column(this, id, columns[id]));
      if (hiddenColumns.indexOf(id) > -1) {
        this.columns.get(id).toggleColumn();
      }
    }
    this.sortedOn = sortOn;
    this.sortBy(this.sortedOn);
    this.populateMenuPopup();
  },

  /**
   * Returns true if the passed string or the row json object corresponds to the
   * selected item in the table.
   */
  isSelected: function(item) {
    if (typeof item == "object") {
      item = item[this.uniqueId];
    }

    return this.selectedRow && item == this.selectedRow[this.uniqueId];
  },

  /**
   * Selects the row corresponding to the `id` json.
   */
  selectRow: function(id) {
    this.selectedRow = id;
  },

  /**
   * Selects the next row. Cycles over to the first row if last row is selected
   */
  selectNextRow: function() {
    for (let column of this.columns.values()) {
      column.selectNextRow();
    }
  },

  /**
   * Selects the previous row. Cycles over to the last row if first row is selected
   */
  selectPreviousRow: function() {
    for (let column of this.columns.values()) {
      column.selectPreviousRow();
    }
  },

  /**
   * Clears any selected row.
   */
  clearSelection: function() {
    this.selectedIndex = -1;
  },

  /**
   * Adds a row into the table.
   *
   * @param {object} item
   *        The object from which the key-value pairs will be taken and added
   *        into the row. This object can have any arbitarary key value pairs,
   *        but only those will be used whose keys match to the ids of the
   *        columns.
   * @param {boolean} suppressFlash
   *        true to not flash the row while inserting the row.
   */
  push: function(item, suppressFlash) {
    if (!this.sortedOn || !this.columns) {
      Cu.reportError("Can't insert item without defining columns first");
      return;
    }

    if (this.items.has(item[this.uniqueId])) {
      this.update(item);
      return;
    }

    let index = this.columns.get(this.sortedOn).push(item);
    for (let [key, column] of this.columns) {
      if (key != this.sortedOn) {
        column.insertAt(item, index);
      }
    }
    this.items.set(item[this.uniqueId], item);
    this.tbody.removeAttribute("empty");
    if (!suppressFlash) {
      this.emit(EVENTS.ROW_UPDATED, item[this.uniqueId]);
    }
  },

  /**
   * Removes the row associated with the `item` object.
   */
  remove: function(item) {
    if (typeof item == "string") {
      item = this.items.get(item);
    }
    let removed = this.items.delete(item[this.uniqueId]);

    if (!removed) {
      return;
    }

    for (let column of this.columns.values()) {
      column.remove(item);
    }
    if (this.items.size == 0) {
      this.tbody.setAttribute("empty", "empty");
    }
  },

  /**
   * Updates the items in the row corresponding to the `item` object previously
   * used to insert the row using `push` method. The linking is done via the
   * `uniqueId` key's value.
   */
  update: function(item) {
    let oldItem = this.items.get(item[this.uniqueId]);
    if (!oldItem) {
      return;
    }
    this.items.set(item[this.uniqueId], item);

    let changed = false;
    for (let column of this.columns.values()) {
      if (item[column.id] != oldItem[column.id]) {
        column.update(item);
        changed = true;
      }
    }
    if (changed) {
      this.emit(EVENTS.ROW_UPDATED, item[this.uniqueId]);
    }
  },

  /**
   * Removes all of the rows from the table.
   */
  clear: function() {
    this.items.clear();
    for (let column of this.columns.values()) {
      column.clear();
    }
    this.tbody.setAttribute("empty", "empty");
    this.setPlaceholderText(this.emptyText);
  },

  /**
   * Sorts the table by a given column.
   *
   * @param {string} column
   *        The id of the column on which the table should be sorted.
   */
  sortBy: function(column) {
    this.emit(EVENTS.COLUMN_SORTED, column);
    this.sortedOn = column;

    if (!this.items.size) {
      return;
    }

    let sortedItems = this.columns.get(column).sort([...this.items.values()]);
    for (let [id, column] of this.columns) {
      if (id != column) {
        column.sort(sortedItems);
      }
    }
  }
};

TableWidget.EVENTS = EVENTS;

module.exports.TableWidget = TableWidget;

/**
 * A single column object in the table.
 *
 * @param {TableWidget} table
 *        The table object to which the column belongs.
 * @param {string} id
 *        Id of the column.
 * @param {String} header
 *        The displayed string on the column's header.
 */
function Column(table, id, header) {
  this.tbody = table.tbody;
  this.document = table.document;
  this.window = table.window;
  this.id = id;
  this.uniqueId = table.uniqueId;
  this.table = table;
  this.cells = [];
  this.items = {};

  this.highlightUpdated = table.highlightUpdated;

  // This wrapping element is required solely so that position:sticky works on
  // the headers of the columns.
  this.wrapper = this.document.createElementNS(XUL_NS, "vbox");
  this.wrapper.className = "table-widget-wrapper";
  this.wrapper.setAttribute("flex", "1");
  this.wrapper.setAttribute("tabindex", "0");
  this.tbody.appendChild(this.wrapper);

  this.splitter = this.document.createElementNS(XUL_NS, "splitter");
  this.splitter.className = "devtools-side-splitter";
  this.tbody.appendChild(this.splitter);

  this.column = this.document.createElementNS(HTML_NS, "div");
  this.column.id = id;
  this.column.className = "table-widget-column";
  this.wrapper.appendChild(this.column);

  this.header = this.document.createElementNS(XUL_NS, "label");
  this.header.className = "plain devtools-toolbar table-widget-column-header";
  this.header.setAttribute("value", header);
  this.column.appendChild(this.header);
  if (table.headersContextMenu) {
    this.header.setAttribute("context", table.headersContextMenu);
  }
  this.toggleColumn = this.toggleColumn.bind(this);
  this.table.on(EVENTS.HEADER_CONTEXT_MENU, this.toggleColumn);

  this.onColumnSorted = this.onColumnSorted.bind(this);
  this.table.on(EVENTS.COLUMN_SORTED, this.onColumnSorted);

  this.onRowUpdated = this.onRowUpdated.bind(this);
  this.table.on(EVENTS.ROW_UPDATED, this.onRowUpdated);

  this.onClick = this.onClick.bind(this);
  this.onMousedown = this.onMousedown.bind(this);
  this.onKeydown = this.onKeydown.bind(this);
  this.column.addEventListener("click", this.onClick);
  this.column.addEventListener("mousedown", this.onMousedown);
  this.column.addEventListener("keydown", this.onKeydown);
}

Column.prototype = {

  // items is a cell-id to cell-index map. It is basically a reverse map of the
  // this.cells object and is used to quickly reverse lookup a cell by its id
  // instead of looping through the cells array. This reverse map is not kept
  // upto date in sync with the cells array as updating it is in itself a loop
  // through all the cells of the columns. Thus update it on demand when it goes
  // out of sync with this.cells.
  items: null,

  // _itemsDirty is a flag which becomes true when this.items goes out of sync
  // with this.cells
  _itemsDirty: null,

  selectedRow: null,

  cells: null,

  /**
   * Gets whether the table is sorted on this column or not.
   * 0 - not sorted.
   * 1 - ascending order
   * 2 - descending order
   */
  get sorted() {
    return this._sortState || 0;
  },

  /**
   * Sets the sorted value
   */
  set sorted(value) {
    if (!value) {
      this.header.removeAttribute("sorted");
    } else {
      this.header.setAttribute("sorted",
        value == 1 ? "ascending" : "descending");
    }
    this._sortState = value;
  },

  /**
   * Gets the selected row in the column.
   */
  get selectedIndex() {
    if (!this.selectedRow) {
      return -1;
    }
    return this.items[this.selectedRow];
  },

  /**
   * Called when the column is sorted by.
   *
   * @param {string} event
   *        The event name of the event. i.e. EVENTS.COLUMN_SORTED
   * @param {string} column
   *        The id of the column being sorted by.
   */
  onColumnSorted: function(event, column) {
    if (column != this.id) {
      this.sorted = 0;
      return;
    } else if (this.sorted == 0 || this.sorted == 2) {
      this.sorted = 1;
    } else {
      this.sorted = 2;
    }
  },

  /**
   * Called when a row is updated.
   *
   * @param {string} event
   *        The event name of the event. i.e. EVENTS.ROW_UPDATED
   * @param {string} id
   *        The unique id of the object associated with the row.
   */
  onRowUpdated: function(event, id) {
    this._updateItems();
    if (this.highlightUpdated && this.items[id] != null) {
      this.cells[this.items[id]].flash();
    }
  },

  destroy: function() {
    this.table.off(EVENTS.COLUMN_SORTED, this.onColumnSorted);
    this.table.off(EVENTS.HEADER_CONTEXT_MENU, this.toggleColumn);
    this.table.off(EVENTS.ROW_UPDATED, this.onRowUpdated);
    this.splitter.remove();
    this.column.parentNode.remove();
    this.cells = null;
    this.items = null;
    this.selectedRow = null;
  },

  /**
   * Selects the row at the `index` index
   */
  selectRowAt: function(index) {
    if (this.selectedRow != null) {
      this.cells[this.items[this.selectedRow]].toggleClass("theme-selected");
    }
    if (index < 0) {
      this.selectedRow = null;
      return;
    }
    let cell = this.cells[index];
    cell.toggleClass("theme-selected");
    cell.focus();
    this.selectedRow = cell.id;
  },

  /**
   * Selects the row with the object having the `uniqueId` value as `id`
   */
  selectRow: function(id) {
    this._updateItems();
    this.selectRowAt(this.items[id]);
  },

  /**
   * Selects the next row. Cycles to first if last row is selected.
   */
  selectNextRow: function() {
    this._updateItems();
    let index = this.items[this.selectedRow] + 1;
    if (index == this.cells.length) {
      index = 0;
    }
    this.selectRowAt(index);
  },

  /**
   * Selects the previous row. Cycles to last if first row is selected.
   */
  selectPreviousRow: function() {
    this._updateItems();
    let index = this.items[this.selectedRow] - 1;
    if (index == -1) {
      index = this.cells.length - 1;
    }
    this.selectRowAt(index);
  },

  /**
   * Pushes the `item` object into the column. If this column is sorted on,
   * then inserts the object at the right position based on the column's id key's
   * value.
   *
   * @returns {number}
   *          The index of the currently pushed item.
   */
  push: function(item) {
    let value = item[this.id];

    if (this.sorted) {
      let index;
      if (this.sorted == 1) {
        index = this.cells.findIndex(element => {
          return value < element.value;
        });
      } else {
        index = this.cells.findIndex(element => {
          return value > element.value;
        });
      }
      index = index >= 0 ? index : this.cells.length;
      if (index < this.cells.length) {
        this._itemsDirty = true;
      }
      this.items[item[this.uniqueId]] = index;
      this.cells.splice(index, 0, new Cell(this, item, this.cells[index]));
      return index;
    }

    this.items[item[this.uniqueId]] = this.cells.length;
    return this.cells.push(new Cell(this, item)) - 1;
  },

  /**
   * Inserts the `item` object at the given `index` index in the table.
   */
  insertAt: function(item, index) {
    if (index < this.cells.length) {
      this._itemsDirty = true;
    }
    this.items[item[this.uniqueId]] = index;
    this.cells.splice(index, 0, new Cell(this, item, this.cells[index]));
  },

  /**
   * Event handler for the command event coming from the header context menu.
   * Toggles the column if it was requested by the user.
   * When called explicitly without parameters, it toggles the corresponding
   * column.
   *
   * @param {string} event
   *        The name of the event. i.e. EVENTS.HEADER_CONTEXT_MENU
   * @param {string} id
   *        Id of the column to be toggled
   * @param {string} checked
   *        true if the column is visible
   */
  toggleColumn: function(event, id, checked) {
    if (arguments.length == 0) {
      // Act like a toggling method when called with no params
      id = this.id;
      checked = this.wrapper.hasAttribute("hidden");
    }
    if (id != this.id) {
      return;
    }
    if (checked) {
      this.wrapper.removeAttribute("hidden");
    } else {
      this.wrapper.setAttribute("hidden", "true");
    }
  },

  /**
   * Removes the corresponding item from the column.
   */
  remove: function(item) {
    this._updateItems();
    let index = this.items[item[this.uniqueId]];
    if (index == null) {
      return;
    }

    if (index < this.cells.length) {
      this._itemsDirty = true;
    }
    this.cells[index].destroy();
    this.cells.splice(index, 1);
    delete this.items[item[this.uniqueId]];
  },

  /**
   * Updates the corresponding item from the column.
   */
  update: function(item) {
    this._updateItems();

    let index = this.items[item[this.uniqueId]];
    if (index == null) {
      return;
    }

    this.cells[index].value = item[this.id];
  },

  /**
   * Updates the `this.items` cell-id vs cell-index map to be in sync with
   * `this.cells`.
   */
  _updateItems: function() {
    if (!this._itemsDirty) {
      return;
    }
    for (let i = 0; i < this.cells.length; i++) {
      this.items[this.cells[i].id] = i;
    }
    this._itemsDirty = false;
  },

  /**
   * Clears the current column
   */
  clear: function() {
    this.cells = [];
    this.items = {};
    this._itemsDirty = false;
    this.selectedRow = null;
    while (this.header.nextSibling) {
      this.header.nextSibling.remove();
    }
  },

  /**
   * Sorts the given items and returns the sorted list if the table was sorted
   * by this column.
   */
  sort: function(items) {
    // Only sort the array if we are sorting based on this column
    if (this.sorted == 1) {
      items.sort((a, b) => {
        let val1 = (a[this.id] instanceof Ci.nsIDOMNode) ?
            a[this.id].textContent : a[this.id];
        let val2 = (b[this.id] instanceof Ci.nsIDOMNode) ?
            b[this.id].textContent : b[this.id];
        return val1 > val2;
      });
    } else if (this.sorted > 1) {
      items.sort((a, b) => {
        let val1 = (a[this.id] instanceof Ci.nsIDOMNode) ?
            a[this.id].textContent : a[this.id];
        let val2 = (b[this.id] instanceof Ci.nsIDOMNode) ?
            b[this.id].textContent : b[this.id];
        return val2 > val1;
      });
    }

    if (this.selectedRow) {
      this.cells[this.items[this.selectedRow]].toggleClass("theme-selected");
    }
    this.items = {};
    // Otherwise, just use the sorted array passed to update the cells value.
    items.forEach((item, i) => {
      this.items[item[this.uniqueId]] = i;
      this.cells[i].value = item[this.id];
      this.cells[i].id = item[this.uniqueId];
    });
    if (this.selectedRow) {
      this.cells[this.items[this.selectedRow]].toggleClass("theme-selected");
    }
    this._itemsDirty = false;
    return items;
  },

  /**
   * Click event handler for the column. Used to detect click on header for
   * for sorting.
   */
  onClick: function(event) {
    if (event.originalTarget == this.column) {
      return;
    }

    if (event.button == 0 && event.originalTarget == this.header) {
      return this.table.sortBy(this.id);
    }
  },

  /**
   * Mousedown event handler for the column. Used to select rows.
   */
  onMousedown: function(event) {
    if (event.originalTarget == this.column ||
        event.originalTarget == this.header) {
      return;
    }
    if (event.button == 0) {
      let target = event.originalTarget;
      let dataid = null;

      while (target) {
        dataid = target.getAttribute("data-id");
        if (dataid) {
          break;
        }
        target = target.parentNode;
      }

      this.table.emit(EVENTS.ROW_SELECTED, dataid);
    }
  },

  /**
   * Keydown event handler for the column. Used for keyboard navigation amongst
   * rows.
   */
  onKeydown: function(event) {
    if (event.originalTarget == this.column ||
        event.originalTarget == this.header) {
      return;
    }

    switch (event.keyCode) {
      case event.DOM_VK_ESCAPE:
      case event.DOM_VK_LEFT:
      case event.DOM_VK_RIGHT:
        return;
      case event.DOM_VK_HOME:
      case event.DOM_VK_END:
        return;
      case event.DOM_VK_UP:
        event.preventDefault();
        let prevRow = event.originalTarget.previousSibling;
        if (this.header == prevRow) {
          prevRow = this.column.lastChild;
        }
        this.table.emit(EVENTS.ROW_SELECTED, prevRow.getAttribute("data-id"));
        break;

      case event.DOM_VK_DOWN:
        event.preventDefault();
        let nextRow = event.originalTarget.nextSibling ||
                      this.header.nextSibling;
        this.table.emit(EVENTS.ROW_SELECTED, nextRow.getAttribute("data-id"));
        break;
    }
  }
};

/**
 * A single cell in a column
 *
 * @param {Column} column
 *        The column object to which the cell belongs.
 * @param {object} item
 *        The object representing the row. It contains a key value pair
 *        representing the column id and its associated value. The value
 *        can be a DOMNode that is appended or a string value.
 * @param {Cell} nextCell
 *        The cell object which is next to this cell. null if this cell is last
 *        cell of the column
 */
function Cell(column, item, nextCell) {
  let document = column.document;

  this.label = document.createElementNS(XUL_NS, "label");
  this.label.setAttribute("crop", "end");
  this.label.className = "plain table-widget-cell";
  if (nextCell) {
    column.column.insertBefore(this.label, nextCell.label);
  } else {
    column.column.appendChild(this.label);
  }

  this.value = item[column.id];
  this.id = item[column.uniqueId];
}

Cell.prototype = {

  set id(value) {
    this._id = value;
    this.label.setAttribute("data-id", value);
  },

  get id() {
    return this._id;
  },

  set value(value) {
    this._value = value;
    if (value == null) {
      this.label.setAttribute("value", "");
      return;
    }

    if (!(value instanceof Ci.nsIDOMNode) &&
        value.length > MAX_VISIBLE_STRING_SIZE) {
      value = value .substr(0, MAX_VISIBLE_STRING_SIZE) + "\u2026"; // …
    }

    if (value instanceof Ci.nsIDOMNode) {
      this.label.removeAttribute("value");

      while (this.label.firstChild) {
        this.label.removeChild(this.label.firstChild);
      }

      this.label.appendChild(value);
    } else {
      this.label.setAttribute("value", value + "");
    }
  },

  get value() {
    return this._value;
  },

  toggleClass: function(className) {
    this.label.classList.toggle(className);
  },

  /**
   * Flashes the cell for a brief time. This when done for ith cells in all
   * columns, makes it look like the row is being highlighted/flashed.
   */
  flash: function() {
    this.label.classList.remove("flash-out");
    // Cause a reflow so that the animation retriggers on adding back the class
    let a = this.label.parentNode.offsetWidth;
    this.label.classList.add("flash-out");
  },

  focus: function() {
    this.label.focus();
  },

  destroy: function() {
    this.label.remove();
    this.label = null;
  }
}
