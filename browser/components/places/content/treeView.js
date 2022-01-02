/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from controller.js */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * This returns the key for any node/details object.
 *
 * @param {object} nodeOrDetails
 *        A node, or an object containing the following properties:
 *        - uri
 *        - time
 *        - itemId
 *        In case any of these is missing, an empty string will be returned. This is
 *        to facilitate easy delete statements which occur due to assignment to items in `this._rows`,
 *        since the item we are deleting may be undefined in the array.
 *
 * @returns {string} key or empty string.
 */
function makeNodeDetailsKey(nodeOrDetails) {
  if (
    nodeOrDetails &&
    typeof nodeOrDetails === "object" &&
    "uri" in nodeOrDetails &&
    "time" in nodeOrDetails &&
    "itemId" in nodeOrDetails
  ) {
    return `${nodeOrDetails.uri}*${nodeOrDetails.time}*${nodeOrDetails.itemId}`;
  }
  return "";
}

function PlacesTreeView(aContainer) {
  this._tree = null;
  this._result = null;
  this._selection = null;
  this._rootNode = null;
  this._rows = [];
  this._flatList = aContainer.flatList;
  this._nodeDetails = new Map();
  this._element = aContainer;
  this._controller = aContainer._controller;
}

PlacesTreeView.prototype = {
  get wrappedJSObject() {
    return this;
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsITreeView",
    "nsINavHistoryResultObserver",
    "nsISupportsWeakReference",
  ]),

  /**
   * This is called once both the result and the tree are set.
   */
  _finishInit: function PTV__finishInit() {
    let selection = this.selection;
    if (selection) {
      selection.selectEventsSuppressed = true;
    }

    if (!this._rootNode.containerOpen) {
      // This triggers containerStateChanged which then builds the visible
      // section.
      this._rootNode.containerOpen = true;
    } else {
      this.invalidateContainer(this._rootNode);
    }

    // "Activate" the sorting column and update commands.
    this.sortingChanged(this._result.sortingMode);

    if (selection) {
      selection.selectEventsSuppressed = false;
    }
  },

  uninit() {
    if (this._editingObservers) {
      for (let observer of this._editingObservers.values()) {
        observer.disconnect();
      }
      delete this._editingObservers;
    }
  },

  /**
   * Plain Container: container result nodes which may never include sub
   * hierarchies.
   *
   * When the rows array is constructed, we don't set the children of plain
   * containers.  Instead, we keep placeholders for these children.  We then
   * build these children lazily as the tree asks us for information about each
   * row.  Luckily, the tree doesn't ask about rows outside the visible area.
   *
   * @see _getNodeForRow and _getRowForNode for the actual magic.
   *
   * @note It's guaranteed that all containers are listed in the rows
   * elements array.  It's also guaranteed that separators (if they're not
   * filtered, see below) are listed in the visible elements array, because
   * bookmark folders are never built lazily, as described above.
   *
   * @param {object} aContainer
   *        A container result node.
   *
   * @returns {boolean} true if aContainer is a plain container, false otherwise.
   */
  _isPlainContainer: function PTV__isPlainContainer(aContainer) {
    // We don't know enough about non-query containers.
    if (!(aContainer instanceof Ci.nsINavHistoryQueryResultNode)) {
      return false;
    }

    switch (aContainer.queryOptions.resultType) {
      case Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY:
      case Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY:
      case Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY:
      case Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAGS_ROOT:
      case Ci.nsINavHistoryQueryOptions.RESULTS_AS_ROOTS_QUERY:
      case Ci.nsINavHistoryQueryOptions.RESULTS_AS_LEFT_PANE_QUERY:
        return false;
    }

    // If it's a folder, it's not a plain container.
    let nodeType = aContainer.type;
    return (
      nodeType != Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER &&
      nodeType != Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT
    );
  },

  /**
   * Gets the row number for a given node.  Assumes that the given node is
   * visible (i.e. it's not an obsolete node).
   *
   * @param {object} aNode
   *        A result node.  Do not pass an obsolete node, or any
   *        node which isn't supposed to be in the tree (e.g. separators in
   *        sorted trees).
   * @param {boolean} [aForceBuild]
   *        @see _isPlainContainer.
   *        If true, the row will be computed even if the node still isn't set
   *        in our rows array.
   * @param {object} [aParentRow]
   *        The row of aNode's parent. Ignored for the root node.
   * @param {number} [aNodeIndex]
   *        The index of aNode in its parent.  Only used if aParentRow is
   *        set too.
   *
   * @throws if aNode is invisible.
   * @note If aParentRow and aNodeIndex are passed and parent is a plain
   * container, this method will just return a calculated row value, without
   * making assumptions on existence of the node at that position.
   * @returns {object} aNode's row if it's in the rows list or if aForceBuild is set, -1
   *         otherwise.
   */
  _getRowForNode: function PTV__getRowForNode(
    aNode,
    aForceBuild,
    aParentRow,
    aNodeIndex
  ) {
    if (aNode == this._rootNode) {
      throw new Error("The root node is never visible");
    }

    // A node is removed form the view either if it has no parent or if its
    // root-ancestor is not the root node (in which case that's the node
    // for which nodeRemoved was called).
    let ancestors = Array.from(PlacesUtils.nodeAncestors(aNode));
    if (
      !ancestors.length ||
      ancestors[ancestors.length - 1] != this._rootNode
    ) {
      throw new Error("Removed node passed to _getRowForNode");
    }

    // Ensure that the entire chain is open, otherwise that node is invisible.
    for (let ancestor of ancestors) {
      if (!ancestor.containerOpen) {
        throw new Error("Invisible node passed to _getRowForNode");
      }
    }

    // Non-plain containers are initially built with their contents.
    let parent = aNode.parent;
    let parentIsPlain = this._isPlainContainer(parent);
    if (!parentIsPlain) {
      if (parent == this._rootNode) {
        return this._rows.indexOf(aNode);
      }

      return this._rows.indexOf(aNode, aParentRow);
    }

    let row = -1;
    let useNodeIndex = typeof aNodeIndex == "number";
    if (parent == this._rootNode) {
      row = useNodeIndex ? aNodeIndex : this._rootNode.getChildIndex(aNode);
    } else if (useNodeIndex && typeof aParentRow == "number") {
      // If we have both the row of the parent node, and the node's index, we
      // can avoid searching the rows array if the parent is a plain container.
      row = aParentRow + aNodeIndex + 1;
    } else {
      // Look for the node in the nodes array.  Start the search at the parent
      // row.  If the parent row isn't passed, we'll pass undefined to indexOf,
      // which is fine.
      row = this._rows.indexOf(aNode, aParentRow);
      if (row == -1 && aForceBuild) {
        let parentRow =
          typeof aParentRow == "number"
            ? aParentRow
            : this._getRowForNode(parent);
        row = parentRow + parent.getChildIndex(aNode) + 1;
      }
    }

    if (row != -1) {
      this._nodeDetails.delete(makeNodeDetailsKey(this._rows[row]));
      this._nodeDetails.set(makeNodeDetailsKey(aNode), aNode);
      this._rows[row] = aNode;
    }

    return row;
  },

  /**
   * Given a row, finds and returns the parent details of the associated node.
   *
   * @param {number} aChildRow
   *        Row number.
   * @returns {array} [parentNode, parentRow]
   */
  _getParentByChildRow: function PTV__getParentByChildRow(aChildRow) {
    let node = this._getNodeForRow(aChildRow);
    let parent = node === null ? this._rootNode : node.parent;

    // The root node is never visible
    if (parent == this._rootNode) {
      return [this._rootNode, -1];
    }

    let parentRow = this._rows.lastIndexOf(parent, aChildRow - 1);
    return [parent, parentRow];
  },

  /**
   * Gets the node at a given row.
   *
   * @param {number} aRow
   * @returns {object}
   */
  _getNodeForRow: function PTV__getNodeForRow(aRow) {
    if (aRow < 0) {
      return null;
    }

    let node = this._rows[aRow];
    if (node !== undefined) {
      return node;
    }

    // Find the nearest node.
    let rowNode, row;
    for (let i = aRow - 1; i >= 0 && rowNode === undefined; i--) {
      rowNode = this._rows[i];
      row = i;
    }

    // If there's no container prior to the given row, it's a child of
    // the root node (remember: all containers are listed in the rows array).
    if (!rowNode) {
      let newNode = this._rootNode.getChild(aRow);
      this._nodeDetails.delete(makeNodeDetailsKey(this._rows[aRow]));
      this._nodeDetails.set(makeNodeDetailsKey(newNode), newNode);
      return (this._rows[aRow] = newNode);
    }

    // Unset elements may exist only in plain containers.  Thus, if the nearest
    // node is a container, it's the row's parent, otherwise, it's a sibling.
    if (rowNode instanceof Ci.nsINavHistoryContainerResultNode) {
      let newNode = rowNode.getChild(aRow - row - 1);
      this._nodeDetails.delete(makeNodeDetailsKey(this._rows[aRow]));
      this._nodeDetails.set(makeNodeDetailsKey(newNode), newNode);
      return (this._rows[aRow] = newNode);
    }

    let [parent, parentRow] = this._getParentByChildRow(row);
    let newNode = parent.getChild(aRow - parentRow - 1);
    this._nodeDetails.delete(makeNodeDetailsKey(this._rows[aRow]));
    this._nodeDetails.set(makeNodeDetailsKey(newNode), newNode);
    return (this._rows[aRow] = newNode);
  },

  /**
   * This takes a container and recursively appends our rows array per its
   * contents.  Assumes that the rows arrays has no rows for the given
   * container.
   *
   * @param {object} aContainer
   *        A container result node.
   * @param {object} aFirstChildRow
   *        The first row at which nodes may be inserted to the row array.
   *        In other words, that's aContainer's row + 1.
   * @param {array} aToOpen
   *        An array of containers to open once the build is done (out param)
   *
   * @returns {number} the number of rows which were inserted.
   */
  _buildVisibleSection: function PTV__buildVisibleSection(
    aContainer,
    aFirstChildRow,
    aToOpen
  ) {
    // There's nothing to do if the container is closed.
    if (!aContainer.containerOpen) {
      return 0;
    }

    // Inserting the new elements into the rows array in one shot (by
    // Array.prototype.concat) is faster than resizing the array (by splice) on each loop
    // iteration.
    let cc = aContainer.childCount;
    let newElements = new Array(cc);
    // We need to clean up the node details from aFirstChildRow + 1 to the end of rows.
    for (let i = aFirstChildRow + 1; i < this._rows.length; i++) {
      this._nodeDetails.delete(makeNodeDetailsKey(this._rows[i]));
    }
    this._rows = this._rows
      .splice(0, aFirstChildRow)
      .concat(newElements, this._rows);

    if (this._isPlainContainer(aContainer)) {
      return cc;
    }

    let sortingMode = this._result.sortingMode;

    let rowsInserted = 0;
    for (let i = 0; i < cc; i++) {
      let curChild = aContainer.getChild(i);
      let curChildType = curChild.type;

      let row = aFirstChildRow + rowsInserted;

      // Don't display separators when sorted.
      if (curChildType == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR) {
        if (sortingMode != Ci.nsINavHistoryQueryOptions.SORT_BY_NONE) {
          // Remove the element for the filtered separator.
          // Notice that the rows array was initially resized to include all
          // children.
          this._nodeDetails.delete(makeNodeDetailsKey(this._rows[row]));
          this._rows.splice(row, 1);
          continue;
        }
      }

      this._nodeDetails.delete(makeNodeDetailsKey(this._rows[row]));
      this._nodeDetails.set(makeNodeDetailsKey(curChild), curChild);
      this._rows[row] = curChild;
      rowsInserted++;

      // Recursively do containers.
      if (
        !this._flatList &&
        curChild instanceof Ci.nsINavHistoryContainerResultNode
      ) {
        let uri = curChild.uri;
        let isopen = false;

        if (uri) {
          let val = Services.xulStore.getValue(
            document.documentURI,
            PlacesUIUtils.obfuscateUrlForXulStore(uri),
            "open"
          );
          isopen = val == "true";
        }

        if (isopen != curChild.containerOpen) {
          aToOpen.push(curChild);
        } else if (curChild.containerOpen && curChild.childCount > 0) {
          rowsInserted += this._buildVisibleSection(curChild, row + 1, aToOpen);
        }
      }
    }

    return rowsInserted;
  },

  /**
   * This counts how many rows a node takes in the tree.  For containers it
   * will count the node itself plus any child node following it.
   *
   * @param {number} aNodeRow
   * @returns {number}
   */
  _countVisibleRowsForNodeAtRow: function PTV__countVisibleRowsForNodeAtRow(
    aNodeRow
  ) {
    let node = this._rows[aNodeRow];

    // If it's not listed yet, we know that it's a leaf node (instanceof also
    // null-checks).
    if (!(node instanceof Ci.nsINavHistoryContainerResultNode)) {
      return 1;
    }

    let outerLevel = node.indentLevel;
    for (let i = aNodeRow + 1; i < this._rows.length; i++) {
      let rowNode = this._rows[i];
      if (rowNode && rowNode.indentLevel <= outerLevel) {
        return i - aNodeRow;
      }
    }

    // This node plus its children take up the bottom of the list.
    return this._rows.length - aNodeRow;
  },

  _getSelectedNodesInRange: function PTV__getSelectedNodesInRange(
    aFirstRow,
    aLastRow
  ) {
    let selection = this.selection;
    let rc = selection.getRangeCount();
    if (rc == 0) {
      return [];
    }

    // The visible-area borders are needed for checking whether a
    // selected row is also visible.
    let firstVisibleRow = this._tree.getFirstVisibleRow();
    let lastVisibleRow = this._tree.getLastVisibleRow();

    let nodesInfo = [];
    for (let rangeIndex = 0; rangeIndex < rc; rangeIndex++) {
      let min = {},
        max = {};
      selection.getRangeAt(rangeIndex, min, max);

      // If this range does not overlap the replaced chunk, we don't need to
      // persist the selection.
      if (max.value < aFirstRow || min.value > aLastRow) {
        continue;
      }

      let firstRow = Math.max(min.value, aFirstRow);
      let lastRow = Math.min(max.value, aLastRow);
      for (let i = firstRow; i <= lastRow; i++) {
        nodesInfo.push({
          node: this._rows[i],
          oldRow: i,
          wasVisible: i >= firstVisibleRow && i <= lastVisibleRow,
        });
      }
    }

    return nodesInfo;
  },

  /**
   * Tries to find an equivalent node for a node which was removed.  We first
   * look for the original node, in case it was just relocated.  Then, if we
   * that node was not found, we look for a node that has the same itemId, uri
   * and time values.
   *
   * @param {object} aUpdatedContainer
   *        An ancestor of the node which was removed.  It does not have to be
   *        its direct parent.
   * @param {object} aOldNode
   *        The node which was removed.
   *
   * @returns {number} the row number of an equivalent node for aOldOne, if one was
   *         found, -1 otherwise.
   */
  _getNewRowForRemovedNode: function PTV__getNewRowForRemovedNode(
    aUpdatedContainer,
    aOldNode
  ) {
    let parent = aOldNode.parent;
    if (parent) {
      // If the node's parent is still set, the node is not obsolete
      // and we should just find out its new position.
      // However, if any of the node's ancestor is closed, the node is
      // invisible.
      let ancestors = PlacesUtils.nodeAncestors(aOldNode);
      for (let ancestor of ancestors) {
        if (!ancestor.containerOpen) {
          return -1;
        }
      }

      return this._getRowForNode(aOldNode, true);
    }

    // There's a broken edge case here.
    // If a visit appears in two queries, and the second one was
    // the old node, we'll select the first one after refresh.  There's
    // nothing we could do about that, because aOldNode.parent is
    // gone by the time invalidateContainer is called.
    let newNode = this._nodeDetails.get(makeNodeDetailsKey(aOldNode));

    if (!newNode) {
      return -1;
    }

    return this._getRowForNode(newNode, true);
  },

  /**
   * Restores a given selection state as near as possible to the original
   * selection state.
   *
   * @param {array} aNodesInfo
   *        The persisted selection state as returned by
   *        _getSelectedNodesInRange.
   * @param {object} aUpdatedContainer
   *        The container which was updated.
   */
  _restoreSelection: function PTV__restoreSelection(
    aNodesInfo,
    aUpdatedContainer
  ) {
    if (!aNodesInfo.length) {
      return;
    }

    let selection = this.selection;

    // Attempt to ensure that previously-visible selection will be visible
    // if it's re-selected.  However, we can only ensure that for one row.
    let scrollToRow = -1;
    for (let i = 0; i < aNodesInfo.length; i++) {
      let nodeInfo = aNodesInfo[i];
      let row = this._getNewRowForRemovedNode(aUpdatedContainer, nodeInfo.node);
      // Select the found node, if any.
      if (row != -1) {
        selection.rangedSelect(row, row, true);
        if (nodeInfo.wasVisible && scrollToRow == -1) {
          scrollToRow = row;
        }
      }
    }

    // If only one node was previously selected and there's no selection now,
    // select the node at its old row, if any.
    if (aNodesInfo.length == 1 && selection.count == 0) {
      let row = Math.min(aNodesInfo[0].oldRow, this._rows.length - 1);
      if (row != -1) {
        selection.rangedSelect(row, row, true);
        if (aNodesInfo[0].wasVisible && scrollToRow == -1) {
          scrollToRow = aNodesInfo[0].oldRow;
        }
      }
    }

    if (scrollToRow != -1) {
      this._tree.ensureRowIsVisible(scrollToRow);
    }
  },

  _convertPRTimeToString: function PTV__convertPRTimeToString(aTime) {
    const MS_PER_MINUTE = 60000;
    const MS_PER_DAY = 86400000;
    let timeMs = aTime / 1000; // PRTime is in microseconds

    // Date is calculated starting from midnight, so the modulo with a day are
    // milliseconds from today's midnight.
    // getTimezoneOffset corrects that based on local time, notice midnight
    // can have a different offset during DST-change days.
    let dateObj = new Date();
    let now = dateObj.getTime() - dateObj.getTimezoneOffset() * MS_PER_MINUTE;
    let midnight = now - (now % MS_PER_DAY);
    midnight += new Date(midnight).getTimezoneOffset() * MS_PER_MINUTE;

    let timeObj = new Date(timeMs);
    return timeMs >= midnight
      ? this._todayFormatter.format(timeObj)
      : this._dateFormatter.format(timeObj);
  },

  // We use a different formatter for times within the current day,
  // so we cache both a "today" formatter and a general date formatter.
  __todayFormatter: null,
  get _todayFormatter() {
    if (!this.__todayFormatter) {
      const dtOptions = { timeStyle: "short" };
      this.__todayFormatter = new Services.intl.DateTimeFormat(
        undefined,
        dtOptions
      );
    }
    return this.__todayFormatter;
  },

  __dateFormatter: null,
  get _dateFormatter() {
    if (!this.__dateFormatter) {
      const dtOptions = {
        dateStyle: "short",
        timeStyle: "short",
      };
      this.__dateFormatter = new Services.intl.DateTimeFormat(
        undefined,
        dtOptions
      );
    }
    return this.__dateFormatter;
  },

  COLUMN_TYPE_UNKNOWN: 0,
  COLUMN_TYPE_TITLE: 1,
  COLUMN_TYPE_URI: 2,
  COLUMN_TYPE_DATE: 3,
  COLUMN_TYPE_VISITCOUNT: 4,
  COLUMN_TYPE_DATEADDED: 5,
  COLUMN_TYPE_LASTMODIFIED: 6,
  COLUMN_TYPE_TAGS: 7,

  _getColumnType: function PTV__getColumnType(aColumn) {
    let columnType = aColumn.element.getAttribute("anonid") || aColumn.id;

    switch (columnType) {
      case "title":
        return this.COLUMN_TYPE_TITLE;
      case "url":
        return this.COLUMN_TYPE_URI;
      case "date":
        return this.COLUMN_TYPE_DATE;
      case "visitCount":
        return this.COLUMN_TYPE_VISITCOUNT;
      case "dateAdded":
        return this.COLUMN_TYPE_DATEADDED;
      case "lastModified":
        return this.COLUMN_TYPE_LASTMODIFIED;
      case "tags":
        return this.COLUMN_TYPE_TAGS;
    }
    return this.COLUMN_TYPE_UNKNOWN;
  },

  _sortTypeToColumnType: function PTV__sortTypeToColumnType(aSortType) {
    switch (aSortType) {
      case Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING:
        return [this.COLUMN_TYPE_TITLE, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_DESCENDING:
        return [this.COLUMN_TYPE_TITLE, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING:
        return [this.COLUMN_TYPE_DATE, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING:
        return [this.COLUMN_TYPE_DATE, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_URI_ASCENDING:
        return [this.COLUMN_TYPE_URI, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_URI_DESCENDING:
        return [this.COLUMN_TYPE_URI, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_ASCENDING:
        return [this.COLUMN_TYPE_VISITCOUNT, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING:
        return [this.COLUMN_TYPE_VISITCOUNT, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_ASCENDING:
        return [this.COLUMN_TYPE_DATEADDED, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING:
        return [this.COLUMN_TYPE_DATEADDED, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_ASCENDING:
        return [this.COLUMN_TYPE_LASTMODIFIED, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_DESCENDING:
        return [this.COLUMN_TYPE_LASTMODIFIED, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_TAGS_ASCENDING:
        return [this.COLUMN_TYPE_TAGS, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_TAGS_DESCENDING:
        return [this.COLUMN_TYPE_TAGS, true];
    }
    return [this.COLUMN_TYPE_UNKNOWN, false];
  },

  // nsINavHistoryResultObserver
  nodeInserted: function PTV_nodeInserted(aParentNode, aNode, aNewIndex) {
    console.assert(this._result, "Got a notification but have no result!");
    if (!this._tree || !this._result) {
      return;
    }

    // Bail out for hidden separators.
    if (PlacesUtils.nodeIsSeparator(aNode) && this.isSorted()) {
      return;
    }

    let parentRow;
    if (aParentNode != this._rootNode) {
      parentRow = this._getRowForNode(aParentNode);

      // Update parent when inserting the first item, since twisty has changed.
      if (aParentNode.childCount == 1) {
        this._tree.invalidateRow(parentRow);
      }
    }

    // Compute the new row number of the node.
    let row = -1;
    let cc = aParentNode.childCount;
    if (aNewIndex == 0 || this._isPlainContainer(aParentNode) || cc == 0) {
      // We don't need to worry about sub hierarchies of the parent node
      // if it's a plain container, or if the new node is its first child.
      if (aParentNode == this._rootNode) {
        row = aNewIndex;
      } else {
        row = parentRow + aNewIndex + 1;
      }
    } else {
      // Here, we try to find the next visible element in the child list so we
      // can set the new visible index to be right before that.  Note that we
      // have to search down instead of up, because some siblings could have
      // children themselves that would be in the way.
      let separatorsAreHidden =
        PlacesUtils.nodeIsSeparator(aNode) && this.isSorted();
      for (let i = aNewIndex + 1; i < cc; i++) {
        let node = aParentNode.getChild(i);
        if (!separatorsAreHidden || PlacesUtils.nodeIsSeparator(node)) {
          // The children have not been shifted so the next item will have what
          // should be our index.
          row = this._getRowForNode(node, false, parentRow, i);
          break;
        }
      }
      if (row < 0) {
        // At the end of the child list without finding a visible sibling. This
        // is a little harder because we don't know how many rows the last item
        // in our list takes up (it could be a container with many children).
        let prevChild = aParentNode.getChild(aNewIndex - 1);
        let prevIndex = this._getRowForNode(
          prevChild,
          false,
          parentRow,
          aNewIndex - 1
        );
        row = prevIndex + this._countVisibleRowsForNodeAtRow(prevIndex);
      }
    }

    this._nodeDetails.set(makeNodeDetailsKey(aNode), aNode);
    this._rows.splice(row, 0, aNode);
    this._tree.rowCountChanged(row, 1);

    if (
      PlacesUtils.nodeIsContainer(aNode) &&
      PlacesUtils.asContainer(aNode).containerOpen
    ) {
      this.invalidateContainer(aNode);
    }
  },

  /**
   * THIS FUNCTION DOES NOT HANDLE cases where a collapsed node is being
   * removed but the node it is collapsed with is not being removed (this then
   * just swap out the removee with its collapsing partner). The only time
   * when we really remove things is when deleting URIs, which will apply to
   * all collapsees. This function is called sometimes when resorting items.
   * However, we won't do this when sorted by date because dates will never
   * change for visits, and date sorting is the only time things are collapsed.
   *
   * @param {object} aParentNode
   * @param {object} aNode
   * @param {number} aOldIndex
   */
  nodeRemoved: function PTV_nodeRemoved(aParentNode, aNode, aOldIndex) {
    console.assert(this._result, "Got a notification but have no result!");
    if (!this._tree || !this._result) {
      return;
    }

    // XXX bug 517701: We don't know what to do when the root node is removed.
    if (aNode == this._rootNode) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
    }

    // Bail out for hidden separators.
    if (PlacesUtils.nodeIsSeparator(aNode) && this.isSorted()) {
      return;
    }

    let parentRow =
      aParentNode == this._rootNode
        ? undefined
        : this._getRowForNode(aParentNode, true);
    let oldRow = this._getRowForNode(aNode, true, parentRow, aOldIndex);
    if (oldRow < 0) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    // If the node was exclusively selected, the node next to it will be
    // selected.
    let selectNext = false;
    let selection = this.selection;
    if (selection.getRangeCount() == 1) {
      let min = {},
        max = {};
      selection.getRangeAt(0, min, max);
      if (min.value == max.value && this.nodeForTreeIndex(min.value) == aNode) {
        selectNext = true;
      }
    }

    // Remove the node and its children, if any.
    let count = this._countVisibleRowsForNodeAtRow(oldRow);
    for (let splicedNode of this._rows.splice(oldRow, count)) {
      this._nodeDetails.delete(makeNodeDetailsKey(splicedNode));
    }
    this._tree.rowCountChanged(oldRow, -count);

    // Redraw the parent if its twisty state has changed.
    if (aParentNode != this._rootNode && !aParentNode.hasChildren) {
      parentRow = oldRow - 1;
      this._tree.invalidateRow(parentRow);
    }

    // Restore selection if the node was exclusively selected.
    if (!selectNext) {
      return;
    }

    // Restore selection.
    let rowToSelect = Math.min(oldRow, this._rows.length - 1);
    if (rowToSelect != -1) {
      this.selection.rangedSelect(rowToSelect, rowToSelect, true);
    }
  },

  nodeMoved: function PTV_nodeMoved(
    aNode,
    aOldParent,
    aOldIndex,
    aNewParent,
    aNewIndex
  ) {
    console.assert(this._result, "Got a notification but have no result!");
    if (!this._tree || !this._result) {
      return;
    }

    // Bail out for hidden separators.
    if (PlacesUtils.nodeIsSeparator(aNode) && this.isSorted()) {
      return;
    }

    // Note that at this point the node has already been moved by the backend,
    // so we must give hints to _getRowForNode to get the old row position.
    let oldParentRow =
      aOldParent == this._rootNode
        ? undefined
        : this._getRowForNode(aOldParent, true);
    let oldRow = this._getRowForNode(aNode, true, oldParentRow, aOldIndex);
    if (oldRow < 0) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    // If this node is a container it could take up more than one row.
    let count = this._countVisibleRowsForNodeAtRow(oldRow);

    // Persist selection state.
    let nodesToReselect = this._getSelectedNodesInRange(
      oldRow,
      oldRow + count - 1
    );
    if (nodesToReselect.length) {
      this.selection.selectEventsSuppressed = true;
    }

    // Redraw the parent if its twisty state has changed.
    if (aOldParent != this._rootNode && !aOldParent.hasChildren) {
      let parentRow = oldRow - 1;
      this._tree.invalidateRow(parentRow);
    }

    // Remove node and its children, if any, from the old position.
    for (let splicedNode of this._rows.splice(oldRow, count)) {
      this._nodeDetails.delete(makeNodeDetailsKey(splicedNode));
    }
    this._tree.rowCountChanged(oldRow, -count);

    // Insert the node into the new position.
    this.nodeInserted(aNewParent, aNode, aNewIndex);

    // Restore selection.
    if (nodesToReselect.length) {
      this._restoreSelection(nodesToReselect, aNewParent);
      this.selection.selectEventsSuppressed = false;
    }
  },

  _invalidateCellValue: function PTV__invalidateCellValue(aNode, aColumnType) {
    console.assert(this._result, "Got a notification but have no result!");
    if (!this._tree || !this._result) {
      return;
    }

    // Nothing to do for the root node.
    if (aNode == this._rootNode) {
      return;
    }

    let row = this._getRowForNode(aNode);
    if (row == -1) {
      return;
    }

    let column = this._findColumnByType(aColumnType);
    if (column && !column.element.hidden) {
      if (aColumnType == this.COLUMN_TYPE_TITLE) {
        this._tree.removeImageCacheEntry(row, column);
      }
      this._tree.invalidateCell(row, column);
    }

    // Last modified time is altered for almost all node changes.
    if (aColumnType != this.COLUMN_TYPE_LASTMODIFIED) {
      let lastModifiedColumn = this._findColumnByType(
        this.COLUMN_TYPE_LASTMODIFIED
      );
      if (lastModifiedColumn && !lastModifiedColumn.hidden) {
        this._tree.invalidateCell(row, lastModifiedColumn);
      }
    }
  },

  nodeTitleChanged: function PTV_nodeTitleChanged(aNode, aNewTitle) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_TITLE);
  },

  nodeURIChanged: function PTV_nodeURIChanged(aNode, aOldURI) {
    this._nodeDetails.delete(
      makeNodeDetailsKey({
        uri: aOldURI,
        itemId: aNode.itemId,
        time: aNode.time,
      })
    );
    this._nodeDetails.set(makeNodeDetailsKey(aNode), aNode);
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_URI);
  },

  nodeIconChanged: function PTV_nodeIconChanged(aNode) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_TITLE);
  },

  nodeHistoryDetailsChanged: function PTV_nodeHistoryDetailsChanged(
    aNode,
    aOldVisitDate,
    aOldVisitCount
  ) {
    this._nodeDetails.delete(
      makeNodeDetailsKey({
        uri: aNode.uri,
        itemId: aNode.itemId,
        time: aOldVisitDate,
      })
    );
    this._nodeDetails.set(makeNodeDetailsKey(aNode), aNode);

    this._invalidateCellValue(aNode, this.COLUMN_TYPE_DATE);
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_VISITCOUNT);
  },

  nodeTagsChanged: function PTV_nodeTagsChanged(aNode) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_TAGS);
  },

  nodeKeywordChanged(aNode, aNewKeyword) {},

  nodeDateAddedChanged: function PTV_nodeDateAddedChanged(aNode, aNewValue) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_DATEADDED);
  },

  nodeLastModifiedChanged: function PTV_nodeLastModifiedChanged(
    aNode,
    aNewValue
  ) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_LASTMODIFIED);
  },

  containerStateChanged: function PTV_containerStateChanged(
    aNode,
    aOldState,
    aNewState
  ) {
    this.invalidateContainer(aNode);
  },

  invalidateContainer: function PTV_invalidateContainer(aContainer) {
    console.assert(this._result, "Need to have a result to update");
    if (!this._tree) {
      return;
    }

    // If we are currently editing, don't invalidate the container until we
    // finish.
    if (this._tree.getAttribute("editing")) {
      if (!this._editingObservers) {
        this._editingObservers = new Map();
      }
      if (!this._editingObservers.has(aContainer)) {
        let mutationObserver = new MutationObserver(() => {
          Services.tm.dispatchToMainThread(() =>
            this.invalidateContainer(aContainer)
          );
          let observer = this._editingObservers.get(aContainer);
          observer.disconnect();
          this._editingObservers.delete(aContainer);
        });

        mutationObserver.observe(this._tree, {
          attributes: true,
          attributeFilter: ["editing"],
        });

        this._editingObservers.set(aContainer, mutationObserver);
      }
      return;
    }

    let startReplacement, replaceCount;
    if (aContainer == this._rootNode) {
      startReplacement = 0;
      replaceCount = this._rows.length;

      // If the root node is now closed, the tree is empty.
      if (!this._rootNode.containerOpen) {
        this._nodeDetails.clear();
        this._rows = [];
        if (replaceCount) {
          this._tree.rowCountChanged(startReplacement, -replaceCount);
        }

        return;
      }
    } else {
      // Update the twisty state.
      let row = this._getRowForNode(aContainer);
      this._tree.invalidateRow(row);

      // We don't replace the container node itself, so we should decrease the
      // replaceCount by 1.
      startReplacement = row + 1;
      replaceCount = this._countVisibleRowsForNodeAtRow(row) - 1;
    }

    // Persist selection state.
    let nodesToReselect = this._getSelectedNodesInRange(
      startReplacement,
      startReplacement + replaceCount
    );

    // Now update the number of elements.
    this.selection.selectEventsSuppressed = true;

    // First remove the old elements
    for (let splicedNode of this._rows.splice(startReplacement, replaceCount)) {
      this._nodeDetails.delete(makeNodeDetailsKey(splicedNode));
    }

    // If the container is now closed, we're done.
    if (!aContainer.containerOpen) {
      let oldSelectionCount = this.selection.count;
      if (replaceCount) {
        this._tree.rowCountChanged(startReplacement, -replaceCount);
      }

      // Select the row next to the closed container if any of its
      // children were selected, and nothing else is selected.
      if (
        nodesToReselect.length &&
        nodesToReselect.length == oldSelectionCount
      ) {
        this.selection.rangedSelect(startReplacement, startReplacement, true);
        this._tree.ensureRowIsVisible(startReplacement);
      }

      this.selection.selectEventsSuppressed = false;
      return;
    }

    // Otherwise, start a batch first.
    this._tree.beginUpdateBatch();
    if (replaceCount) {
      this._tree.rowCountChanged(startReplacement, -replaceCount);
    }

    let toOpenElements = [];
    let elementsAddedCount = this._buildVisibleSection(
      aContainer,
      startReplacement,
      toOpenElements
    );
    if (elementsAddedCount) {
      this._tree.rowCountChanged(startReplacement, elementsAddedCount);
    }

    if (!this._flatList) {
      // Now, open any containers that were persisted.
      for (let i = 0; i < toOpenElements.length; i++) {
        let item = toOpenElements[i];
        let parent = item.parent;

        // Avoid recursively opening containers.
        while (parent) {
          if (parent.uri == item.uri) {
            break;
          }
          parent = parent.parent;
        }

        // If we don't have a parent, we made it all the way to the root
        // and didn't find a match, so we can open our item.
        if (!parent && !item.containerOpen) {
          item.containerOpen = true;
        }
      }
    }

    this._tree.endUpdateBatch();

    // Restore selection.
    this._restoreSelection(nodesToReselect, aContainer);
    this.selection.selectEventsSuppressed = false;
  },

  _columns: [],
  _findColumnByType: function PTV__findColumnByType(aColumnType) {
    if (this._columns[aColumnType]) {
      return this._columns[aColumnType];
    }

    let columns = this._tree.columns;
    let colCount = columns.count;
    for (let i = 0; i < colCount; i++) {
      let column = columns.getColumnAt(i);
      let columnType = this._getColumnType(column);
      this._columns[columnType] = column;
      if (columnType == aColumnType) {
        return column;
      }
    }

    // That's completely valid.  Most of our trees actually include just the
    // title column.
    return null;
  },

  sortingChanged: function PTV__sortingChanged(aSortingMode) {
    if (!this._tree || !this._result) {
      return;
    }

    // Depending on the sort mode, certain commands may be disabled.
    window.updateCommands("sort");

    let columns = this._tree.columns;

    // Clear old sorting indicator.
    let sortedColumn = columns.getSortedColumn();
    if (sortedColumn) {
      sortedColumn.element.removeAttribute("sortDirection");
    }

    // Set new sorting indicator by looking through all columns for ours.
    if (aSortingMode == Ci.nsINavHistoryQueryOptions.SORT_BY_NONE) {
      return;
    }

    let [desiredColumn, desiredIsDescending] = this._sortTypeToColumnType(
      aSortingMode
    );
    let column = this._findColumnByType(desiredColumn);
    if (column) {
      let sortDir = desiredIsDescending ? "descending" : "ascending";
      column.element.setAttribute("sortDirection", sortDir);
    }
  },

  _inBatchMode: false,
  batching: function PTV__batching(aToggleMode) {
    if (this._inBatchMode != aToggleMode) {
      this._inBatchMode = this.selection.selectEventsSuppressed = aToggleMode;
      if (this._inBatchMode) {
        this._tree.beginUpdateBatch();
      } else {
        this._tree.endUpdateBatch();
      }
    }
  },

  get result() {
    return this._result;
  },
  set result(val) {
    if (this._result) {
      this._result.removeObserver(this);
      this._rootNode.containerOpen = false;
    }

    if (val) {
      this._result = val;
      this._rootNode = this._result.root;
      this._cellProperties = new Map();
      this._cuttingNodes = new Set();
    } else if (this._result) {
      delete this._result;
      delete this._rootNode;
      delete this._cellProperties;
      delete this._cuttingNodes;
    }

    // If the tree is not set yet, setTree will call finishInit.
    if (this._tree && val) {
      this._finishInit();
    }
  },

  /**
   * This allows you to get at the real node for a given row index. This is
   * only valid when a tree is attached.
   *
   * @param {Integer} aIndex The index for the node to get.
   * @returns {Ci.nsINavHistoryResultNode} The node.
   * @throws Cr.NS_ERROR_INVALID_ARG if the index is greater than the number of
   *                                 rows.
   */
  nodeForTreeIndex(aIndex) {
    if (aIndex > this._rows.length) {
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }

    return this._getNodeForRow(aIndex);
  },

  /**
   * Reverse of nodeForTreeIndex, returns the row index for a given result node.
   * The node should be part of the tree.
   *
   * @param {Ci.nsINavHistoryResultNode} aNode The node to look for in the tree.
   * @returns {Integer} The found index, or -1 if the item is not visible or not found.
   */
  treeIndexForNode(aNode) {
    // The API allows passing invisible nodes.
    try {
      return this._getRowForNode(aNode, true);
    } catch (ex) {}

    return -1;
  },

  // nsITreeView
  get rowCount() {
    return this._rows.length;
  },
  get selection() {
    return this._selection;
  },
  set selection(val) {
    this._selection = val;
  },

  getRowProperties() {
    return "";
  },

  getCellProperties: function PTV_getCellProperties(aRow, aColumn) {
    // for anonid-trees, we need to add the column-type manually
    var props = "";
    let columnType = aColumn.element.getAttribute("anonid");
    if (columnType) {
      props += columnType;
    } else {
      columnType = aColumn.id;
    }

    // Set the "ltr" property on url cells
    if (columnType == "url") {
      props += " ltr";
    }

    if (columnType != "title") {
      return props;
    }

    let node = this._getNodeForRow(aRow);

    if (this._cuttingNodes.has(node)) {
      props += " cutting";
    }

    let properties = this._cellProperties.get(node);
    if (properties === undefined) {
      properties = "";
      let itemId = node.itemId;
      let nodeType = node.type;
      if (PlacesUtils.containerTypes.includes(nodeType)) {
        if (nodeType == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY) {
          properties += " query";
          if (PlacesUtils.nodeIsTagQuery(node)) {
            properties += " tagContainer";
          } else if (PlacesUtils.nodeIsDay(node)) {
            properties += " dayContainer";
          } else if (PlacesUtils.nodeIsHost(node)) {
            properties += " hostContainer";
          }
        }

        if (itemId == -1) {
          switch (node.bookmarkGuid) {
            case PlacesUtils.bookmarks.virtualToolbarGuid:
              properties += ` queryFolder_${PlacesUtils.bookmarks.toolbarGuid}`;
              break;
            case PlacesUtils.bookmarks.virtualMenuGuid:
              properties += ` queryFolder_${PlacesUtils.bookmarks.menuGuid}`;
              break;
            case PlacesUtils.bookmarks.virtualUnfiledGuid:
              properties += ` queryFolder_${PlacesUtils.bookmarks.unfiledGuid}`;
              break;
            case PlacesUtils.virtualAllBookmarksGuid:
            case PlacesUtils.virtualHistoryGuid:
            case PlacesUtils.virtualDownloadsGuid:
            case PlacesUtils.virtualTagsGuid:
              properties += ` OrganizerQuery_${node.bookmarkGuid}`;
              break;
          }
        }
      } else if (nodeType == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR) {
        properties += " separator";
      } else if (PlacesUtils.nodeIsURI(node)) {
        properties += " " + PlacesUIUtils.guessUrlSchemeForUI(node.uri);
      }

      this._cellProperties.set(node, properties);
    }

    return props + " " + properties;
  },

  getColumnProperties(aColumn) {
    return "";
  },

  isContainer: function PTV_isContainer(aRow) {
    // Only leaf nodes aren't listed in the rows array.
    let node = this._rows[aRow];
    if (node === undefined || !PlacesUtils.nodeIsContainer(node)) {
      return false;
    }

    // Flat-lists may ignore expandQueries and other query options when
    // they are asked to open a container.
    if (this._flatList) {
      return true;
    }

    // Treat non-expandable childless queries as non-containers, unless they
    // are tags.
    if (PlacesUtils.nodeIsQuery(node) && !PlacesUtils.nodeIsTagQuery(node)) {
      return (
        PlacesUtils.asQuery(node).queryOptions.expandQueries || node.hasChildren
      );
    }
    return true;
  },

  isContainerOpen: function PTV_isContainerOpen(aRow) {
    if (this._flatList) {
      return false;
    }

    // All containers are listed in the rows array.
    return this._rows[aRow].containerOpen;
  },

  isContainerEmpty: function PTV_isContainerEmpty(aRow) {
    if (this._flatList) {
      return true;
    }

    // All containers are listed in the rows array.
    return !this._rows[aRow].hasChildren;
  },

  isSeparator: function PTV_isSeparator(aRow) {
    // All separators are listed in the rows array.
    let node = this._rows[aRow];
    return node && PlacesUtils.nodeIsSeparator(node);
  },

  isSorted: function PTV_isSorted() {
    return (
      this._result.sortingMode != Ci.nsINavHistoryQueryOptions.SORT_BY_NONE
    );
  },

  canDrop: function PTV_canDrop(aRow, aOrientation, aDataTransfer) {
    if (!this._result) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    if (this._controller.disableUserActions) {
      return false;
    }

    // Drop position into a sorted treeview would be wrong.
    if (this.isSorted()) {
      return false;
    }

    let ip = this._getInsertionPoint(aRow, aOrientation);
    return ip && PlacesControllerDragHelper.canDrop(ip, aDataTransfer);
  },

  _getInsertionPoint: function PTV__getInsertionPoint(index, orientation) {
    let container = this._result.root;
    let dropNearNode = null;
    // When there's no selection, assume the container is the container
    // the view is populated from (i.e. the result's itemId).
    if (index != -1) {
      let lastSelected = this.nodeForTreeIndex(index);
      if (this.isContainer(index) && orientation == Ci.nsITreeView.DROP_ON) {
        // If the last selected item is an open container, append _into_
        // it, rather than insert adjacent to it.
        container = lastSelected;
        index = -1;
      } else if (
        lastSelected.containerOpen &&
        orientation == Ci.nsITreeView.DROP_AFTER &&
        lastSelected.hasChildren
      ) {
        // If the last selected node is an open container and the user is
        // trying to drag into it as a first node, really insert into it.
        container = lastSelected;
        orientation = Ci.nsITreeView.DROP_ON;
        index = 0;
      } else {
        // Use the last-selected node's container.
        container = lastSelected.parent;

        // During its Drag & Drop operation, the tree code closes-and-opens
        // containers very often (part of the XUL "spring-loaded folders"
        // implementation).  And in certain cases, we may reach a closed
        // container here.  However, we can simply bail out when this happens,
        // because we would then be back here in less than a millisecond, when
        // the container had been reopened.
        if (!container || !container.containerOpen) {
          return null;
        }

        // Avoid the potentially expensive call to getChildIndex
        // if we know this container doesn't allow insertion.
        if (this._controller.disallowInsertion(container)) {
          return null;
        }

        let queryOptions = PlacesUtils.asQuery(this._result.root).queryOptions;
        if (
          queryOptions.sortingMode != Ci.nsINavHistoryQueryOptions.SORT_BY_NONE
        ) {
          // If we are within a sorted view, insert at the end.
          index = -1;
        } else if (queryOptions.excludeItems || queryOptions.excludeQueries) {
          // Some item may be invisible, insert near last selected one.
          // We don't replace index here to avoid requests to the db,
          // instead it will be calculated later by the controller.
          index = -1;
          dropNearNode = lastSelected;
        } else {
          let lsi = container.getChildIndex(lastSelected);
          index = orientation == Ci.nsITreeView.DROP_BEFORE ? lsi : lsi + 1;
        }
      }
    }

    if (this._controller.disallowInsertion(container)) {
      return null;
    }

    let tagName = PlacesUtils.nodeIsTagQuery(container)
      ? PlacesUtils.asQuery(container).query.tags[0]
      : null;

    return new PlacesInsertionPoint({
      parentId: PlacesUtils.getConcreteItemId(container),
      parentGuid: PlacesUtils.getConcreteItemGuid(container),
      index,
      orientation,
      tagName,
      dropNearNode,
    });
  },

  drop: function PTV_drop(aRow, aOrientation, aDataTransfer) {
    if (this._controller.disableUserActions) {
      return;
    }

    // We are responsible for translating the |index| and |orientation|
    // parameters into a container id and index within the container,
    // since this information is specific to the tree view.
    let ip = this._getInsertionPoint(aRow, aOrientation);
    if (ip) {
      PlacesControllerDragHelper.onDrop(ip, aDataTransfer, this._tree)
        .catch(Cu.reportError)
        .then(() => {
          // We should only clear the drop target once
          // the onDrop is complete, as it is an async function.
          PlacesControllerDragHelper.currentDropTarget = null;
        });
    }
  },

  getParentIndex: function PTV_getParentIndex(aRow) {
    let [, parentRow] = this._getParentByChildRow(aRow);
    return parentRow;
  },

  hasNextSibling: function PTV_hasNextSibling(aRow, aAfterIndex) {
    if (aRow == this._rows.length - 1) {
      // The last row has no sibling.
      return false;
    }

    let node = this._rows[aRow];
    if (node === undefined || this._isPlainContainer(node.parent)) {
      // The node is a child of a plain container.
      // If the next row is either unset or has the same parent,
      // it's a sibling.
      let nextNode = this._rows[aRow + 1];
      return nextNode == undefined || nextNode.parent == node.parent;
    }

    let thisLevel = node.indentLevel;
    for (let i = aAfterIndex + 1; i < this._rows.length; ++i) {
      let rowNode = this._getNodeForRow(i);
      let nextLevel = rowNode.indentLevel;
      if (nextLevel == thisLevel) {
        return true;
      }
      if (nextLevel < thisLevel) {
        break;
      }
    }

    return false;
  },

  getLevel(aRow) {
    return this._getNodeForRow(aRow).indentLevel;
  },

  getImageSrc: function PTV_getImageSrc(aRow, aColumn) {
    // Only the title column has an image.
    if (this._getColumnType(aColumn) != this.COLUMN_TYPE_TITLE) {
      return "";
    }

    let node = this._getNodeForRow(aRow);
    return node.icon;
  },

  getCellValue(aRow, aColumn) {},

  getCellText: function PTV_getCellText(aRow, aColumn) {
    let node = this._getNodeForRow(aRow);
    switch (this._getColumnType(aColumn)) {
      case this.COLUMN_TYPE_TITLE:
        // normally, this is just the title, but we don't want empty items in
        // the tree view so return a special string if the title is empty.
        // Do it here so that callers can still get at the 0 length title
        // if they go through the "result" API.
        if (PlacesUtils.nodeIsSeparator(node)) {
          return "";
        }
        return PlacesUIUtils.getBestTitle(node, true);
      case this.COLUMN_TYPE_TAGS:
        return node.tags;
      case this.COLUMN_TYPE_URI:
        if (PlacesUtils.nodeIsURI(node)) {
          return node.uri;
        }
        return "";
      case this.COLUMN_TYPE_DATE:
        let nodeTime = node.time;
        if (nodeTime == 0 || !PlacesUtils.nodeIsURI(node)) {
          // hosts and days shouldn't have a value for the date column.
          // Actually, you could argue this point, but looking at the
          // results, seeing the most recently visited date is not what
          // I expect, and gives me no information I know how to use.
          // Only show this for URI-based items.
          return "";
        }

        return this._convertPRTimeToString(nodeTime);
      case this.COLUMN_TYPE_VISITCOUNT:
        return node.accessCount;
      case this.COLUMN_TYPE_DATEADDED:
        if (node.dateAdded) {
          return this._convertPRTimeToString(node.dateAdded);
        }
        return "";
      case this.COLUMN_TYPE_LASTMODIFIED:
        if (node.lastModified) {
          return this._convertPRTimeToString(node.lastModified);
        }
        return "";
    }
    return "";
  },

  setTree: function PTV_setTree(aTree) {
    // If we are replacing the tree during a batch, there is a concrete risk
    // that the treeView goes out of sync, thus it's safer to end the batch now.
    // This is a no-op if we are not batching.
    this.batching(false);

    let hasOldTree = this._tree != null;
    this._tree = aTree;

    if (this._result) {
      if (hasOldTree) {
        // detach from result when we are detaching from the tree.
        // This breaks the reference cycle between us and the result.
        if (!aTree) {
          // Balances the addObserver call from the load method in tree.xml
          this._result.removeObserver(this);
          this._rootNode.containerOpen = false;
        }
      }
      if (aTree) {
        this._finishInit();
      }
    }
  },

  toggleOpenState: function PTV_toggleOpenState(aRow) {
    if (!this._result) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    let node = this._rows[aRow];
    if (this._flatList && this._element) {
      let event = new CustomEvent("onOpenFlatContainer", { detail: node });
      this._element.dispatchEvent(event);
      return;
    }

    let uri = node.uri;

    if (uri) {
      let docURI = document.documentURI;

      if (node.containerOpen) {
        Services.xulStore.removeValue(
          docURI,
          PlacesUIUtils.obfuscateUrlForXulStore(uri),
          "open"
        );
      } else {
        Services.xulStore.setValue(
          docURI,
          PlacesUIUtils.obfuscateUrlForXulStore(uri),
          "open",
          "true"
        );
      }
    }

    node.containerOpen = !node.containerOpen;
  },

  cycleHeader: function PTV_cycleHeader(aColumn) {
    if (!this._result) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    // Sometimes you want a tri-state sorting, and sometimes you don't. This
    // rule allows tri-state sorting when the root node is a folder. This will
    // catch the most common cases. When you are looking at folders, you want
    // the third state to reset the sorting to the natural bookmark order. When
    // you are looking at history, that third state has no meaning so we try
    // to disallow it.
    //
    // The problem occurs when you have a query that results in bookmark
    // folders. One example of this is the subscriptions view. In these cases,
    // this rule doesn't allow you to sort those sub-folders by their natural
    // order.
    let allowTriState = PlacesUtils.nodeIsFolder(this._result.root);

    let oldSort = this._result.sortingMode;
    let newSort;
    const NHQO = Ci.nsINavHistoryQueryOptions;
    switch (this._getColumnType(aColumn)) {
      case this.COLUMN_TYPE_TITLE:
        if (oldSort == NHQO.SORT_BY_TITLE_ASCENDING) {
          newSort = NHQO.SORT_BY_TITLE_DESCENDING;
        } else if (allowTriState && oldSort == NHQO.SORT_BY_TITLE_DESCENDING) {
          newSort = NHQO.SORT_BY_NONE;
        } else {
          newSort = NHQO.SORT_BY_TITLE_ASCENDING;
        }

        break;
      case this.COLUMN_TYPE_URI:
        if (oldSort == NHQO.SORT_BY_URI_ASCENDING) {
          newSort = NHQO.SORT_BY_URI_DESCENDING;
        } else if (allowTriState && oldSort == NHQO.SORT_BY_URI_DESCENDING) {
          newSort = NHQO.SORT_BY_NONE;
        } else {
          newSort = NHQO.SORT_BY_URI_ASCENDING;
        }

        break;
      case this.COLUMN_TYPE_DATE:
        if (oldSort == NHQO.SORT_BY_DATE_ASCENDING) {
          newSort = NHQO.SORT_BY_DATE_DESCENDING;
        } else if (allowTriState && oldSort == NHQO.SORT_BY_DATE_DESCENDING) {
          newSort = NHQO.SORT_BY_NONE;
        } else {
          newSort = NHQO.SORT_BY_DATE_ASCENDING;
        }

        break;
      case this.COLUMN_TYPE_VISITCOUNT:
        // visit count default is unusual because we sort by descending
        // by default because you are most likely to be looking for
        // highly visited sites when you click it
        if (oldSort == NHQO.SORT_BY_VISITCOUNT_DESCENDING) {
          newSort = NHQO.SORT_BY_VISITCOUNT_ASCENDING;
        } else if (
          allowTriState &&
          oldSort == NHQO.SORT_BY_VISITCOUNT_ASCENDING
        ) {
          newSort = NHQO.SORT_BY_NONE;
        } else {
          newSort = NHQO.SORT_BY_VISITCOUNT_DESCENDING;
        }

        break;
      case this.COLUMN_TYPE_DATEADDED:
        if (oldSort == NHQO.SORT_BY_DATEADDED_ASCENDING) {
          newSort = NHQO.SORT_BY_DATEADDED_DESCENDING;
        } else if (
          allowTriState &&
          oldSort == NHQO.SORT_BY_DATEADDED_DESCENDING
        ) {
          newSort = NHQO.SORT_BY_NONE;
        } else {
          newSort = NHQO.SORT_BY_DATEADDED_ASCENDING;
        }

        break;
      case this.COLUMN_TYPE_LASTMODIFIED:
        if (oldSort == NHQO.SORT_BY_LASTMODIFIED_ASCENDING) {
          newSort = NHQO.SORT_BY_LASTMODIFIED_DESCENDING;
        } else if (
          allowTriState &&
          oldSort == NHQO.SORT_BY_LASTMODIFIED_DESCENDING
        ) {
          newSort = NHQO.SORT_BY_NONE;
        } else {
          newSort = NHQO.SORT_BY_LASTMODIFIED_ASCENDING;
        }

        break;
      case this.COLUMN_TYPE_TAGS:
        if (oldSort == NHQO.SORT_BY_TAGS_ASCENDING) {
          newSort = NHQO.SORT_BY_TAGS_DESCENDING;
        } else if (allowTriState && oldSort == NHQO.SORT_BY_TAGS_DESCENDING) {
          newSort = NHQO.SORT_BY_NONE;
        } else {
          newSort = NHQO.SORT_BY_TAGS_ASCENDING;
        }

        break;
      default:
        throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }
    this._result.sortingMode = newSort;
  },

  isEditable: function PTV_isEditable(aRow, aColumn) {
    // At this point we only support editing the title field.
    if (aColumn.index != 0) {
      return false;
    }

    let node = this._rows[aRow];
    if (!node) {
      Cu.reportError("isEditable called for an unbuilt row.");
      return false;
    }
    let itemGuid = node.bookmarkGuid;

    // Only bookmark-nodes are editable.
    if (!itemGuid) {
      return false;
    }

    // The following items are also not editable, even though they are bookmark
    // items.
    // * places-roots
    // * the left pane special folders and queries (those are place: uri
    //   bookmarks)
    // * separators
    //
    // Note that concrete itemIds aren't used intentionally.  For example, we
    // have no reason to disallow renaming a shortcut to the Bookmarks Toolbar,
    // except for the one under All Bookmarks.
    if (
      PlacesUtils.nodeIsSeparator(node) ||
      PlacesUtils.isRootItem(itemGuid) ||
      PlacesUtils.isQueryGeneratedFolder(node)
    ) {
      return false;
    }

    return true;
  },

  setCellText: function PTV_setCellText(aRow, aColumn, aText) {
    // We may only get here if the cell is editable.
    let node = this._rows[aRow];
    if (node.title != aText) {
      PlacesTransactions.EditTitle({ guid: node.bookmarkGuid, title: aText })
        .transact()
        .catch(Cu.reportError);
    }
  },

  toggleCutNode: function PTV_toggleCutNode(aNode, aValue) {
    let currentVal = this._cuttingNodes.has(aNode);
    if (currentVal != aValue) {
      if (aValue) {
        this._cuttingNodes.add(aNode);
      } else {
        this._cuttingNodes.delete(aNode);
      }

      this._invalidateCellValue(aNode, this.COLUMN_TYPE_TITLE);
    }
  },

  selectionChanged() {},
  cycleCell(aRow, aColumn) {},
};
