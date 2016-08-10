/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "setNamedTimeout",
  "devtools/client/shared/widgets/view-helpers", true);
loader.lazyRequireGetter(this, "clearNamedTimeout",
  "devtools/client/shared/widgets/view-helpers", true);

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HTML_NS = "http://www.w3.org/1999/xhtml";
const AFTER_SCROLL_DELAY = 100;

// Different types of events emitted by the Various components of the
// TableWidget.
const EVENTS = {
  CELL_EDIT: "cell-edit",
  COLUMN_SORTED: "column-sorted",
  COLUMN_TOGGLED: "column-toggled",
  FIELDS_EDITABLE: "fields-editable",
  HEADER_CONTEXT_MENU: "header-context-menu",
  ROW_EDIT: "row-edit",
  ROW_CONTEXT_MENU: "row-context-menu",
  ROW_REMOVED: "row-removed",
  ROW_SELECTED: "row-selected",
  ROW_UPDATED: "row-updated",
  TABLE_CLEARED: "table-cleared",
  TABLE_FILTERED: "table-filtered",
  SCROLL_END: "scroll-end"
};
Object.defineProperty(this, "EVENTS", {
  value: EVENTS,
  enumerable: true,
  writable: false
});

// Maximum number of character visible in any cell in the table. This is to
// avoid making the cell take up all the space in a row.
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
 *        - wrapTextInElements: Don't ever use 'value' attribute on labels.
 *                              Default: false.
 *        - emptyText: text to display when no entries in the table to display.
 *        - highlightUpdated: true to highlight the changed/added row.
 *        - removableColumns: Whether columns are removeable. If set to false,
 *                            the context menu in the headers will not appear.
 *        - firstColumn: key of the first column that should appear.
 *        - cellContextMenuId: ID of a <menupopup> element to be set as a
 *                             context menu of every cell.
 */
function TableWidget(node, options = {}) {
  EventEmitter.decorate(this);

  this.document = node.ownerDocument;
  this.window = this.document.defaultView;
  this._parent = node;

  let {initialColumns, emptyText, uniqueId, highlightUpdated, removableColumns,
       firstColumn, wrapTextInElements, cellContextMenuId} = options;
  this.emptyText = emptyText || "";
  this.uniqueId = uniqueId || "name";
  this.wrapTextInElements = wrapTextInElements || false;
  this.firstColumn = firstColumn || "";
  this.highlightUpdated = highlightUpdated || false;
  this.removableColumns = removableColumns !== false;
  this.cellContextMenuId = cellContextMenuId;

  this.tbody = this.document.createElementNS(XUL_NS, "hbox");
  this.tbody.className = "table-widget-body theme-body";
  this.tbody.setAttribute("flex", "1");
  this.tbody.setAttribute("tabindex", "0");
  this._parent.appendChild(this.tbody);
  this.afterScroll = this.afterScroll.bind(this);
  this.tbody.addEventListener("scroll", this.onScroll.bind(this));

  this.placeholder = this.document.createElementNS(XUL_NS, "label");
  this.placeholder.className = "plain table-widget-empty-text";
  this.placeholder.setAttribute("flex", "1");
  this._parent.appendChild(this.placeholder);

  this.items = new Map();
  this.columns = new Map();

  // Setup the column headers context menu to allow users to hide columns at
  // will.
  if (this.removableColumns) {
    this.onPopupCommand = this.onPopupCommand.bind(this);
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

  this.onChange = this.onChange.bind(this);
  this.onEditorDestroyed = this.onEditorDestroyed.bind(this);
  this.onEditorTab = this.onEditorTab.bind(this);
  this.onKeydown = this.onKeydown.bind(this);
  this.onMousedown = this.onMousedown.bind(this);
  this.onRowRemoved = this.onRowRemoved.bind(this);

  this.document.addEventListener("keydown", this.onKeydown, false);
  this.document.addEventListener("mousedown", this.onMousedown, false);
}

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

  /**
   * Returns the index of the selected row disregarding hidden rows.
   */
  get visibleSelectedIndex() {
    let cells = this.columns.get(this.uniqueId).visibleCellNodes;

    for (let i = 0; i < cells.length; i++) {
      if (cells[i].classList.contains("theme-selected")) {
        return i;
      }
    }

    return -1;
  },

  /**
   * returns all editable columns.
   */
  get editableColumns() {
    let filter = columns => {
      columns = [...columns].filter(col => {
        if (col.clientWidth === 0) {
          return false;
        }

        let cell = col.querySelector(".table-widget-cell");

        for (let selector of this._editableFieldsEngine.selectors) {
          if (cell.matches(selector)) {
            return true;
          }
        }

        return false;
      });

      return columns;
    };

    let columns = this._parent.querySelectorAll(".table-widget-column");
    return filter(columns);
  },

  /**
   * Emit all cell edit events.
   */
  onChange: function (type, data) {
    let changedField = data.change.field;
    let colName = changedField.parentNode.id;
    let column = this.columns.get(colName);
    let uniqueId = column.table.uniqueId;
    let itemIndex = column.cellNodes.indexOf(changedField);
    let items = {};

    for (let [name, col] of this.columns) {
      items[name] = col.cellNodes[itemIndex].value;
    }

    let change = {
      host: this.host,
      key: uniqueId,
      field: colName,
      oldValue: data.change.oldValue,
      newValue: data.change.newValue,
      items: items
    };

    // A rows position in the table can change as the result of an edit. In
    // order to ensure that the correct row is highlighted after an edit we
    // save the uniqueId in editBookmark.
    this.editBookmark = colName === uniqueId ? change.newValue
                                             : items[uniqueId];
    this.emit(EVENTS.CELL_EDIT, change);
  },

  onEditorDestroyed: function () {
    this._editableFieldsEngine = null;
  },

  /**
   * Called by the inplace editor when Tab / Shift-Tab is pressed in edit-mode.
   * Because tables are live any row, column, cell or table can be added,
   * deleted or moved by deleting and adding e.g. a row again.
   *
   * This presents various challenges when navigating via the keyboard so please
   * keep this in mind whenever editing this method.
   *
   * @param  {Event} event
   *         Keydown event
   */
  onEditorTab: function (event) {
    let textbox = event.target;
    let editor = this._editableFieldsEngine;

    if (textbox.id !== editor.INPUT_ID) {
      return;
    }

    let column = textbox.parentNode;

    // Changing any value can change the position of the row depending on which
    // column it is currently sorted on. In addition to this, the table cell may
    // have been edited and had to be recreated when the user has pressed tab or
    // shift+tab. Both of these situations require us to recover our target,
    // select the appropriate row and move the textbox on to the next cell.
    if (editor.changePending) {
      // We need to apply a change, which can mean that the position of cells
      // within the table can change. Because of this we need to wait for
      // EVENTS.ROW_EDIT and then move the textbox.
      this.once(EVENTS.ROW_EDIT, (e, uniqueId) => {
        let cell;
        let cells;
        let columnObj;
        let cols = this.editableColumns;
        let rowIndex = this.visibleSelectedIndex;
        let colIndex = cols.indexOf(column);
        let newIndex;

        // If the row has been deleted we should bail out.
        if (!uniqueId) {
          return;
        }

        // Find the column we need to move to.
        if (event.shiftKey) {
          // Navigate backwards on shift tab.
          if (colIndex === 0) {
            if (rowIndex === 0) {
              return;
            }
            newIndex = cols.length - 1;
          } else {
            newIndex = colIndex - 1;
          }
        } else if (colIndex === cols.length - 1) {
          let id = cols[0].id;
          columnObj = this.columns.get(id);
          let maxRowIndex = columnObj.visibleCellNodes.length - 1;
          if (rowIndex === maxRowIndex) {
            return;
          }
          newIndex = 0;
        } else {
          newIndex = colIndex + 1;
        }

        let newcol = cols[newIndex];
        columnObj = this.columns.get(newcol.id);

        // Select the correct row even if it has moved due to sorting.
        let dataId = editor.currentTarget.getAttribute("data-id");
        if (this.items.get(dataId)) {
          this.emit(EVENTS.ROW_SELECTED, dataId);
        } else {
          this.emit(EVENTS.ROW_SELECTED, uniqueId);
        }

        // EVENTS.ROW_SELECTED may have changed the selected row so let's save
        // the result in rowIndex.
        rowIndex = this.visibleSelectedIndex;

        // Edit the appropriate cell.
        cells = columnObj.visibleCellNodes;
        cell = cells[rowIndex];
        editor.edit(cell);

        // Remove flash-out class... it won't have been auto-removed because the
        // cell was hidden for editing.
        cell.classList.remove("flash-out");
      });
    }

    // Begin cell edit. We always do this so that we can begin editing even in
    // the case that the previous edit will cause the row to move.
    let cell = this.getEditedCellOnTab(event, column);
    editor.edit(cell);
  },

  /**
   * Get the cell that will be edited next on tab / shift tab and highlight the
   * appropriate row. Edits etc. are not taken into account.
   *
   * This is used to tab from one field to another without editing and makes the
   * editor much more responsive.
   *
   * @param  {Event} event
   *         Keydown event
   */
  getEditedCellOnTab: function (event, column) {
    let cell = null;
    let cols = this.editableColumns;
    let rowIndex = this.visibleSelectedIndex;
    let colIndex = cols.indexOf(column);
    let maxCol = cols.length - 1;
    let maxRow = this.columns.get(column.id).visibleCellNodes.length - 1;

    if (event.shiftKey) {
      // Navigate backwards on shift tab.
      if (colIndex === 0) {
        if (rowIndex === 0) {
          this._editableFieldsEngine.completeEdit();
          return null;
        }

        column = cols[cols.length - 1];
        let cells = this.columns.get(column.id).visibleCellNodes;
        cell = cells[rowIndex - 1];

        let rowId = cell.getAttribute("data-id");
        this.emit(EVENTS.ROW_SELECTED, rowId);
      } else {
        column = cols[colIndex - 1];
        let cells = this.columns.get(column.id).visibleCellNodes;
        cell = cells[rowIndex];
      }
    } else if (colIndex === maxCol) {
      // If in the rightmost column on the last row stop editing.
      if (rowIndex === maxRow) {
        this._editableFieldsEngine.completeEdit();
        return null;
      }

      // If in the rightmost column of a row then move to the first column of
      // the next row.
      column = cols[0];
      let cells = this.columns.get(column.id).visibleCellNodes;
      cell = cells[rowIndex + 1];

      let rowId = cell.getAttribute("data-id");
      this.emit(EVENTS.ROW_SELECTED, rowId);
    } else {
      // Navigate forwards on tab.
      column = cols[colIndex + 1];
      let cells = this.columns.get(column.id).visibleCellNodes;
      cell = cells[rowIndex];
    }

    return cell;
  },

  /**
   * Reset the editable fields engine if the currently edited row is removed.
   *
   * @param  {String} event
   *         The event name "event-removed."
   * @param  {Object} row
   *         The values from the removed row.
   */
  onRowRemoved: function (event, row) {
    if (!this._editableFieldsEngine || !this._editableFieldsEngine.isEditing) {
      return;
    }

    let removedKey = row[this.uniqueId];
    let column = this.columns.get(this.uniqueId);

    if (removedKey in column.items) {
      return;
    }

    // The target is lost so we need to hide the remove the textbox from the DOM
    // and reset the target nodes.
    this.onEditorTargetLost();
  },

  /**
   * Cancel an edit because the edit target has been lost.
   */
  onEditorTargetLost: function () {
    let editor = this._editableFieldsEngine;

    if (!editor || !editor.isEditing) {
      return;
    }

    editor.cancelEdit();
  },

  /**
   * Keydown event handler for the table. Used for keyboard navigation amongst
   * rows.
   */
  onKeydown: function (event) {
    // If we are in edit mode bail out.
    if (this._editableFieldsEngine && this._editableFieldsEngine.isEditing) {
      return;
    }

    let selectedCell = this.tbody.querySelector(".theme-selected");
    if (!selectedCell) {
      return;
    }

    let colName;
    let column;
    let visibleCells;
    let index;
    let cell;

    switch (event.keyCode) {
      case event.DOM_VK_UP:
        event.preventDefault();

        colName = selectedCell.parentNode.id;
        column = this.columns.get(colName);
        visibleCells = column.visibleCellNodes;
        index = visibleCells.indexOf(selectedCell);

        if (index > 0) {
          index--;
        } else {
          index = visibleCells.length - 1;
        }

        cell = visibleCells[index];

        this.emit(EVENTS.ROW_SELECTED, cell.getAttribute("data-id"));
        break;
      case event.DOM_VK_DOWN:
        event.preventDefault();

        colName = selectedCell.parentNode.id;
        column = this.columns.get(colName);
        visibleCells = column.visibleCellNodes;
        index = visibleCells.indexOf(selectedCell);

        if (index === visibleCells.length - 1) {
          index = 0;
        } else {
          index++;
        }

        cell = visibleCells[index];

        this.emit(EVENTS.ROW_SELECTED, cell.getAttribute("data-id"));
        break;
    }
  },

  /**
   * Close any editors if the area "outside the table" is clicked. In reality,
   * the table covers the whole area but there are labels filling the top few
   * rows. This method clears any inline editors if an area outside a textbox or
   * label is clicked.
   */
  onMousedown: function ({target}) {
    let nodeName = target.nodeName;

    if (nodeName === "textbox" || !this._editableFieldsEngine) {
      return;
    }

    // Force any editor fields to hide due to XUL focus quirks.
    this._editableFieldsEngine.blur();
  },

  /**
   * Make table fields editable.
   *
   * @param  {String|Array} editableColumns
   *         An array or comma separated list of editable column names.
   */
  makeFieldsEditable: function (editableColumns) {
    let selectors = [];

    if (typeof editableColumns === "string") {
      editableColumns = [editableColumns];
    }

    for (let id of editableColumns) {
      selectors.push("#" + id + " .table-widget-cell");
    }

    for (let [name, column] of this.columns) {
      if (!editableColumns.includes(name)) {
        column.column.setAttribute("readonly", "");
      }
    }

    if (this._editableFieldsEngine) {
      this._editableFieldsEngine.selectors = selectors;
    } else {
      this._editableFieldsEngine = new EditableFieldsEngine({
        root: this.tbody,
        onTab: this.onEditorTab,
        onTriggerEvent: "dblclick",
        selectors: selectors
      });

      this._editableFieldsEngine.on("change", this.onChange);
      this._editableFieldsEngine.on("destroyed", this.onEditorDestroyed);

      this.on(EVENTS.ROW_REMOVED, this.onRowRemoved);
      this.on(EVENTS.TABLE_CLEARED, this._editableFieldsEngine.cancelEdit);

      this.emit(EVENTS.FIELDS_EDITABLE, this._editableFieldsEngine);
    }
  },

  destroy: function () {
    this.off(EVENTS.ROW_SELECTED, this.bindSelectedRow);
    this.off(EVENTS.ROW_REMOVED, this.onRowRemoved);

    this.document.removeEventListener("keydown", this.onKeydown, false);
    this.document.removeEventListener("mousedown", this.onMousedown, false);

    if (this._editableFieldsEngine) {
      this.off(EVENTS.TABLE_CLEARED, this._editableFieldsEngine.cancelEdit);
      this._editableFieldsEngine.off("change", this.onChange);
      this._editableFieldsEngine.off("destroyed", this.onEditorDestroyed);
      this._editableFieldsEngine.destroy();
      this._editableFieldsEngine = null;
    }

    if (this.menupopup) {
      this.menupopup.removeEventListener("command", this.onPopupCommand);
      this.menupopup.remove();
    }
  },

  /**
   * Sets the text to be shown when the table is empty.
   */
  setPlaceholderText: function (text) {
    this.placeholder.setAttribute("value", text);
  },

  /**
   * Prepares the context menu for the headers of the table columns. This
   * context menu allows users to toggle various columns, only with an exception
   * of the unique columns and when only two columns are visible in the table.
   */
  setupHeadersContextMenu: function () {
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
  populateMenuPopup: function () {
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
  onPopupCommand: function (event) {
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
  setColumns: function (columns, sortOn = this.sortedOn, hiddenColumns = []) {
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
  isSelected: function (item) {
    if (typeof item == "object") {
      item = item[this.uniqueId];
    }

    return this.selectedRow && item == this.selectedRow[this.uniqueId];
  },

  /**
   * Selects the row corresponding to the `id` json.
   */
  selectRow: function (id) {
    this.selectedRow = id;
  },

  /**
   * Selects the next row. Cycles over to the first row if last row is selected
   */
  selectNextRow: function () {
    for (let column of this.columns.values()) {
      column.selectNextRow();
    }
  },

  /**
   * Selects the previous row. Cycles over to the last row if first row is
   * selected.
   */
  selectPreviousRow: function () {
    for (let column of this.columns.values()) {
      column.selectPreviousRow();
    }
  },

  /**
   * Clears any selected row.
   */
  clearSelection: function () {
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
  push: function (item, suppressFlash) {
    if (!this.sortedOn || !this.columns) {
      console.error("Can't insert item without defining columns first");
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
      column.updateZebra();
    }
    this.items.set(item[this.uniqueId], item);
    this.tbody.removeAttribute("empty");

    if (!suppressFlash) {
      this.emit(EVENTS.ROW_UPDATED, item[this.uniqueId]);
    }

    this.emit(EVENTS.ROW_EDIT, item[this.uniqueId]);
  },

  /**
   * Removes the row associated with the `item` object.
   */
  remove: function (item) {
    if (typeof item != "object") {
      item = this.items.get(item);
    }
    if (!item) {
      return;
    }
    let removed = this.items.delete(item[this.uniqueId]);

    if (!removed) {
      return;
    }
    for (let column of this.columns.values()) {
      column.remove(item);
      column.updateZebra();
    }
    if (this.items.size == 0) {
      this.tbody.setAttribute("empty", "empty");
    }

    this.emit(EVENTS.ROW_REMOVED, item);
  },

  /**
   * Updates the items in the row corresponding to the `item` object previously
   * used to insert the row using `push` method. The linking is done via the
   * `uniqueId` key's value.
   */
  update: function (item) {
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
      this.emit(EVENTS.ROW_EDIT, item[this.uniqueId]);
    }
  },

  /**
   * Removes all of the rows from the table.
   */
  clear: function () {
    this.items.clear();
    for (let column of this.columns.values()) {
      column.clear();
    }
    this.tbody.setAttribute("empty", "empty");
    this.setPlaceholderText(this.emptyText);

    this.emit(EVENTS.TABLE_CLEARED, this);
  },

  /**
   * Sorts the table by a given column.
   *
   * @param {string} column
   *        The id of the column on which the table should be sorted.
   */
  sortBy: function (column) {
    this.emit(EVENTS.COLUMN_SORTED, column);
    this.sortedOn = column;

    if (!this.items.size) {
      return;
    }

    let sortedItems = this.columns.get(column).sort([...this.items.values()]);
    for (let [id, col] of this.columns) {
      if (id != col) {
        col.sort(sortedItems);
      }
    }
  },

  /**
   * Filters the table based on a specific value
   *
   * @param {String} value: The filter value
   * @param {Array} ignoreProps: Props to ignore while filtering
   */
  filterItems(value, ignoreProps = []) {
    if (this.filteredValue == value) {
      return;
    }
    if (this._editableFieldsEngine) {
      this._editableFieldsEngine.completeEdit();
    }

    this.filteredValue = value;
    if (!value) {
      this.emit(EVENTS.TABLE_FILTERED, []);
      return;
    }
    // Shouldn't be case-sensitive
    value = value.toLowerCase();

    let itemsToHide = [...this.items.keys()];
    // Loop through all items and hide unmatched items
    for (let [id, val] of this.items) {
      for (let prop in val) {
        if (ignoreProps.includes(prop)) {
          continue;
        }
        let propValue = val[prop].toString().toLowerCase();
        if (propValue.includes(value)) {
          itemsToHide.splice(itemsToHide.indexOf(id), 1);
          break;
        }
      }
    }
    this.emit(EVENTS.TABLE_FILTERED, itemsToHide);
  },

  /**
   * Calls the afterScroll function when the user has stopped scrolling
   */
  onScroll: function () {
    clearNamedTimeout("table-scroll");
    setNamedTimeout("table-scroll", AFTER_SCROLL_DELAY, this.afterScroll);
  },

  /**
   * Emits the "scroll-end" event when the whole table is scrolled
   */
  afterScroll: function () {
    let scrollHeight = this.tbody.getBoundingClientRect().height -
        this.tbody.querySelector(".table-widget-column-header").clientHeight;

    // Emit scroll-end event when 9/10 of the table is scrolled
    if (this.tbody.scrollTop >= 0.9 * scrollHeight) {
      this.emit("scroll-end");
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
  this.wrapTextInElements = table.wrapTextInElements;
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
  this.header.className = "devtools-toolbar table-widget-column-header";
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

  this.onTableFiltered = this.onTableFiltered.bind(this);
  this.table.on(EVENTS.TABLE_FILTERED, this.onTableFiltered);

  this.onClick = this.onClick.bind(this);
  this.onMousedown = this.onMousedown.bind(this);
  this.column.addEventListener("click", this.onClick);
  this.column.addEventListener("mousedown", this.onMousedown);
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

  get cellNodes() {
    return [...this.column.querySelectorAll(".table-widget-cell")];
  },

  get visibleCellNodes() {
    let editor = this.table._editableFieldsEngine;
    let nodes = this.cellNodes.filter(node => {
      // If the cell is currently being edited we should class it as visible.
      if (editor && editor.currentTarget === node) {
        return true;
      }
      return node.clientWidth !== 0;
    });

    return nodes;
  },

  /**
   * Called when the column is sorted by.
   *
   * @param {string} event
   *        The event name of the event. i.e. EVENTS.COLUMN_SORTED
   * @param {string} column
   *        The id of the column being sorted by.
   */
  onColumnSorted: function (event, column) {
    if (column != this.id) {
      this.sorted = 0;
      return;
    } else if (this.sorted == 0 || this.sorted == 2) {
      this.sorted = 1;
    } else {
      this.sorted = 2;
    }
    this.updateZebra();
  },

  onTableFiltered: function (event, itemsToHide) {
    this._updateItems();
    if (!this.cells) {
      return;
    }
    for (let cell of this.cells) {
      cell.hidden = false;
    }
    for (let id of itemsToHide) {
      this.cells[this.items[id]].hidden = true;
    }
    this.updateZebra();
  },

  /**
   * Called when a row is updated.
   *
   * @param {string} event
   *        The event name of the event. i.e. EVENTS.ROW_UPDATED
   * @param {string} id
   *        The unique id of the object associated with the row.
   */
  onRowUpdated: function (event, id) {
    this._updateItems();
    if (this.highlightUpdated && this.items[id] != null) {
      if (this.table.editBookmark) {
        // A rows position in the table can change as the result of an edit. In
        // order to ensure that the correct row is highlighted after an edit we
        // save the uniqueId in editBookmark. Here we send the signal that the
        // row has been edited and that the row needs to be selected again.
        this.table.emit(EVENTS.ROW_SELECTED, this.table.editBookmark);
        this.table.editBookmark = null;
      }

      this.cells[this.items[id]].flash();
    }
    this.updateZebra();
  },

  destroy: function () {
    this.table.off(EVENTS.COLUMN_SORTED, this.onColumnSorted);
    this.table.off(EVENTS.HEADER_CONTEXT_MENU, this.toggleColumn);
    this.table.off(EVENTS.ROW_UPDATED, this.onRowUpdated);
    this.table.off(EVENTS.TABLE_FILTERED, this.onTableFiltered);

    this.column.removeEventListener("click", this.onClick);
    this.column.removeEventListener("mousedown", this.onMousedown);

    this.splitter.remove();
    this.column.parentNode.remove();
    this.cells = null;
    this.items = null;
    this.selectedRow = null;
  },

  /**
   * Selects the row at the `index` index
   */
  selectRowAt: function (index) {
    if (this.selectedRow != null) {
      this.cells[this.items[this.selectedRow]].toggleClass("theme-selected");
    }
    if (index < 0) {
      this.selectedRow = null;
      return;
    }
    let cell = this.cells[index];
    cell.toggleClass("theme-selected");
    this.selectedRow = cell.id;
  },

  /**
   * Selects the row with the object having the `uniqueId` value as `id`
   */
  selectRow: function (id) {
    this._updateItems();
    this.selectRowAt(this.items[id]);
  },

  /**
   * Selects the next row. Cycles to first if last row is selected.
   */
  selectNextRow: function () {
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
  selectPreviousRow: function () {
    this._updateItems();
    let index = this.items[this.selectedRow] - 1;
    if (index == -1) {
      index = this.cells.length - 1;
    }
    this.selectRowAt(index);
  },

  /**
   * Pushes the `item` object into the column. If this column is sorted on,
   * then inserts the object at the right position based on the column's id
   * key's value.
   *
   * @returns {number}
   *          The index of the currently pushed item.
   */
  push: function (item) {
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
  insertAt: function (item, index) {
    if (index < this.cells.length) {
      this._itemsDirty = true;
    }
    this.items[item[this.uniqueId]] = index;
    this.cells.splice(index, 0, new Cell(this, item, this.cells[index]));
    this.updateZebra();
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
  toggleColumn: function (event, id, checked) {
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
  remove: function (item) {
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
  update: function (item) {
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
  _updateItems: function () {
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
  clear: function () {
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
  sort: function (items) {
    // Only sort the array if we are sorting based on this column
    if (this.sorted == 1) {
      items.sort((a, b) => {
        let val1 = (a[this.id] instanceof Node) ?
            a[this.id].textContent : a[this.id];
        let val2 = (b[this.id] instanceof Node) ?
            b[this.id].textContent : b[this.id];
        return val1 > val2;
      });
    } else if (this.sorted > 1) {
      items.sort((a, b) => {
        let val1 = (a[this.id] instanceof Node) ?
            a[this.id].textContent : a[this.id];
        let val2 = (b[this.id] instanceof Node) ?
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
    this.updateZebra();
    return items;
  },

  updateZebra() {
    this._updateItems();
    let i = 0;
    for (let cell of this.cells) {
      if (!cell.hidden) {
        i++;
      }
      cell.toggleClass("even", !(i % 2));
    }
  },

  /**
   * Click event handler for the column. Used to detect click on header for
   * for sorting.
   */
  onClick: function (event) {
    let target = event.originalTarget;

    if (target.nodeType !== target.ELEMENT_NODE || target == this.column) {
      return;
    }

    if (event.button == 0 && target == this.header) {
      this.table.sortBy(this.id);
    }
  },

  /**
   * Mousedown event handler for the column. Used to select rows.
   */
  onMousedown: function (event) {
    let target = event.originalTarget;

    if (target.nodeType !== target.ELEMENT_NODE ||
        target == this.column ||
        target == this.header) {
      return;
    }
    if (event.button == 0) {
      let closest = target.closest("[data-id]");
      if (!closest) {
        return;
      }

      let dataid = target.getAttribute("data-id");
      this.table.emit(EVENTS.ROW_SELECTED, dataid);
    }
  },
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

  this.wrapTextInElements = column.wrapTextInElements;
  this.label = document.createElementNS(XUL_NS, "label");
  this.label.setAttribute("crop", "end");
  this.label.className = "plain table-widget-cell";

  if (nextCell) {
    column.column.insertBefore(this.label, nextCell.label);
  } else {
    column.column.appendChild(this.label);
  }

  if (column.table.cellContextMenuId) {
    this.label.setAttribute("context", column.table.cellContextMenuId);
    this.label.addEventListener("contextmenu", (event) => {
      // Make the ID of the clicked cell available as a property on the table.
      // It's then available for the popupshowing or command handler.
      column.table.contextMenuRowId = this.id;
    }, false);
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

  get hidden() {
    return this.label.hasAttribute("hidden");
  },

  set hidden(value) {
    if (value) {
      this.label.setAttribute("hidden", "hidden");
    } else {
      this.label.removeAttribute("hidden");
    }
  },

  set value(value) {
    this._value = value;
    if (value == null) {
      this.label.setAttribute("value", "");
      return;
    }

    if (this.wrapTextInElements && !(value instanceof Node)) {
      let span = this.label.ownerDocument.createElementNS(HTML_NS, "span");
      span.textContent = value;
      value = span;
    }

    if (!(value instanceof Node) &&
        value.length > MAX_VISIBLE_STRING_SIZE) {
      value = value .substr(0, MAX_VISIBLE_STRING_SIZE) + "\u2026";
    }

    if (value instanceof Node) {
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

  toggleClass: function (className, condition) {
    this.label.classList.toggle(className, condition);
  },

  /**
   * Flashes the cell for a brief time. This when done for with cells in all
   * columns, makes it look like the row is being highlighted/flashed.
   */
  flash: function () {
    if (!this.label.parentNode) {
      return;
    }
    this.label.classList.remove("flash-out");
    // Cause a reflow so that the animation retriggers on adding back the class
    let a = this.label.parentNode.offsetWidth; // eslint-disable-line
    let onAnimEnd = () => {
      this.label.classList.remove("flash-out");
      this.label.removeEventListener("animationend", onAnimEnd);
    };
    this.label.addEventListener("animationend", onAnimEnd);
    this.label.classList.add("flash-out");
  },

  focus: function () {
    this.label.focus();
  },

  destroy: function () {
    this.label.remove();
    this.label = null;
  }
};

/**
 * Simple widget to make nodes matching a CSS selector editable.
 *
 * @param {Object} options
 *        An object with the following format:
 *          {
 *            // The node that will act as a container for the editor e.g. a
 *            // div or table.
 *            root: someNode,
 *
 *            // The onTab event to be handled by the caller.
 *            onTab: function(event) { ... }
 *
 *            // Optional event used to trigger the editor. By default this is
 *            // dblclick.
 *            onTriggerEvent: "dblclick",
 *
 *            // Array or comma separated string of CSS Selectors matching
 *            // elements that are to be made editable.
 *            selectors: [
 *              "#name .table-widget-cell",
 *              "#value .table-widget-cell"
 *            ]
 *          }
 */
function EditableFieldsEngine(options) {
  EventEmitter.decorate(this);

  if (!Array.isArray(options.selectors)) {
    options.selectors = [options.selectors];
  }

  this.root = options.root;
  this.selectors = options.selectors;
  this.onTab = options.onTab;
  this.onTriggerEvent = options.onTriggerEvent || "dblclick";

  this.edit = this.edit.bind(this);
  this.cancelEdit = this.cancelEdit.bind(this);
  this.destroy = this.destroy.bind(this);

  this.onTrigger = this.onTrigger.bind(this);
  this.root.addEventListener(this.onTriggerEvent, this.onTrigger);
}

EditableFieldsEngine.prototype = {
  INPUT_ID: "inlineEditor",

  get changePending() {
    return this.isEditing && (this.textbox.value !== this.currentValue);
  },

  get isEditing() {
    return this.root && !this.textbox.hidden;
  },

  get textbox() {
    if (!this._textbox) {
      let doc = this.root.ownerDocument;
      this._textbox = doc.createElementNS(XUL_NS, "textbox");
      this._textbox.id = this.INPUT_ID;

      this._textbox.setAttribute("flex", "1");

      this.onKeydown = this.onKeydown.bind(this);
      this._textbox.addEventListener("keydown", this.onKeydown);

      this.completeEdit = this.completeEdit.bind(this);
      doc.addEventListener("blur", this.completeEdit);
    }

    return this._textbox;
  },

  /**
   * Called when a trigger event is detected (default is dblclick).
   *
   * @param  {EventTarget} target
   *         Calling event's target.
   */
  onTrigger: function ({target}) {
    this.edit(target);
  },

  /**
   * Handle keypresses when in edit mode:
   *   - <escape> revert the value and close the textbox.
   *   - <return> apply the value and close the textbox.
   *   - <tab> Handled by the consumer's `onTab` callback.
   *   - <shift><tab> Handled by the consumer's `onTab` callback.
   *
   * @param  {Event} event
   *         The calling event.
   */
  onKeydown: function (event) {
    if (!this.textbox) {
      return;
    }

    switch (event.keyCode) {
      case event.DOM_VK_ESCAPE:
        this.cancelEdit();
        event.preventDefault();
        break;
      case event.DOM_VK_RETURN:
        this.completeEdit();
        break;
      case event.DOM_VK_TAB:
        if (this.onTab) {
          this.onTab(event);
        }
        break;
    }
  },

  /**
   * Overlay the target node with an edit field.
   *
   * @param  {Node} target
   *         Dom node to be edited.
   */
  edit: function (target) {
    if (!target) {
      return;
    }

    target.scrollIntoView(false);
    target.focus();

    if (!target.matches(this.selectors.join(","))) {
      return;
    }

    // If we are actively editing something complete the edit first.
    if (this.isEditing) {
      this.completeEdit();
    }

    this.copyStyles(target, this.textbox);

    target.parentNode.insertBefore(this.textbox, target);
    this.currentTarget = target;
    this.textbox.value = this.currentValue = target.value;
    target.hidden = true;
    this.textbox.hidden = false;

    this.textbox.focus();
    this.textbox.select();
  },

  completeEdit: function () {
    if (!this.isEditing) {
      return;
    }

    let oldValue = this.currentValue;
    let newValue = this.textbox.value;
    let changed = oldValue !== newValue;

    this.textbox.hidden = true;

    if (!this.currentTarget) {
      return;
    }

    this.currentTarget.hidden = false;
    if (changed) {
      this.currentTarget.value = newValue;

      let data = {
        change: {
          field: this.currentTarget,
          oldValue: oldValue,
          newValue: newValue
        }
      };

      this.emit("change", data);
    }
  },

  /**
   * Cancel an edit.
   */
  cancelEdit: function () {
    if (!this.isEditing) {
      return;
    }
    if (this.currentTarget) {
      this.currentTarget.hidden = false;
    }

    this.textbox.hidden = true;
  },

  /**
   * Stop edit mode and apply changes.
   */
  blur: function () {
    if (this.isEditing) {
      this.completeEdit();
    }
  },

  /**
   * Copies various styles from one node to another.
   *
   * @param  {Node} source
   *         The node to copy styles from.
   * @param  {Node} destination [description]
   *         The node to copy styles to.
   */
  copyStyles: function (source, destination) {
    let style = source.ownerDocument.defaultView.getComputedStyle(source);
    let props = [
      "borderTopWidth",
      "borderRightWidth",
      "borderBottomWidth",
      "borderLeftWidth",
      "fontFamily",
      "fontSize",
      "fontWeight",
      "height",
      "marginTop",
      "marginRight",
      "marginBottom",
      "marginLeft",
      "marginInlineStart",
      "marginInlineEnd"
    ];

    for (let prop of props) {
      destination.style[prop] = style[prop];
    }

    // We need to set the label width to 100% to work around a XUL flex bug.
    destination.style.width = "100%";
  },

  /**
   * Destroys all editors in the current document.
   */
  destroy: function () {
    if (this.textbox) {
      this.textbox.removeEventListener("keydown", this.onKeydown);
      this.textbox.remove();
    }

    if (this.root) {
      this.root.removeEventListener(this.onTriggerEvent, this.onTrigger);
      this.root.ownerDocument.removeEventListener("blur", this.completeEdit);
    }

    this._textbox = this.root = this.selectors = this.onTab = null;
    this.currentTarget = this.currentValue = null;

    this.emit("destroyed");
  },
};
