/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla History System
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *   Asaf Romano <mano@mozilla.com> (Javascript version)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

PlacesTreeView.prototype = {
  _makeAtom: function PTV__makeAtom(aString) {
    return  Cc["@mozilla.org/atom-service;1"].
            getService(Ci.nsIAtomService).
            getAtom(aString);
  },

  __containerAtom: null,
  get _containerAtom() {
    if (!this.__containerAtom)
      this.__containerAtom = this._makeAtom("container");

    return this.__containerAtom;
  },

  __sessionStartAtom: null,
  get _sessionStartAtom() {
    if (!this.__sessionStartAtom)
      this.__sessionStartAtom = this._makeAtom("session-start");

    return this.__sessionStartAtom;
  },

  __sessionContinueAtom: null,
  get _sessionContinueAtom() {
    if (!this.__sessionContinueAtom)
      this.__sessionContinueAtom = this._makeAtom("session-continue");

    return this.__sessionContinueAtom;
  },

  _ensureValidRow: function PTV__ensureValidRow(aRow) {
    if (aRow < 0 || aRow > this._visibleElements.length)
      throw Cr.NS_ERROR_INVALID_ARG;
  },

  __dateService: null,
  get _dateService() {
    if (!this.__dateService) {
      this.__dateService = Cc["@mozilla.org/intl/scriptabledateformat;1"].
                           getService(Ci.nsIScriptableDateFormat);
    }
    return this.__dateService;
  },

  QueryInterface: function PTV_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsITreeView) ||
        aIID.equals(Ci.nsINavHistoryResultViewer) ||
        aIID.equals(Ci.nsINavHistoryResultTreeViewer) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  /**
   * This is called when the result or tree may have changed.
   * It reinitializes everything. Result and/or tree can be null
   * when calling.
   */
  _finishInit: function PTV__finishInit() {
    if (this._tree && this._result)
      this.sortingChanged(this._result.sortingMode);

    // if there is no tree, BuildVisibleList will clear everything for us
    this._buildVisibleList();
  },

  _computeShowSessions: function PTV__computeShowSessions() {
    NS_ASSERT(this._result, "Must have a result to show sessions!");
    this._showSessions = false;

    var options = asQuery(this._result.root).queryOptions;
    NS_ASSERT(options, "navHistoryResults must have valid options");

    if (!options.showSessions)
      return; // sessions are off

    var resultType = options.resultType;
    if (resultType != Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT &&
        resultType != Ci.nsINavHistoryQueryOptions.RESULTS_AS_FULL_VISIT)
      return; // not visits

    var sortType = this._result.sortingMode;
    if (sortType != nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING &&
        sortType != nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING)
      return; // not date sorting

    // showing sessions only makes sense if we are grouping by date
    // any other grouping (or recursive grouping) doesn't make sense
    var groupings = options.getGroupingMode({});
    for (var i=0; i < groupings.length; i++) {
      if (groupings[i] != Ci.nsINavHistoryQueryOptions.GROUP_BY_DAY)
        return; // non-time-based grouping
    }

    this._showSessions = true;
  },

  SESSION_STATUS_NONE: 0,
  SESSION_STATUS_START: 1,
  SESSION_STATUS_CONTINUE: 2,
  _getRowSessionStatus: function PTV__getRowSessionStatus(aRow) {
    this._ensureValidRow(aRow);
    var node = this._visibleElements[aRow];
    if (!PlacesUtils.nodeIsVisit(node) || asVisit(node).sessionId == 0)
      return this.SESSION_STATUS_NONE;

    if (aRow == 0)
      return this.SESSION_STATUS_START;

    var previousNode = this._visibleElements[aRow - 1];
    if (!PlacesUtils.nodeIsVisit(previousNode) ||
        node.sessionId != asVisit(previousNode).sessionId)
      return this.SESSION_STATUS_START;

    return this.SESSION_STATUS_CONTINUE;
  },

  /**
   * Call to completely rebuild the list of visible items. Note if there is no
   * tree or root this will just clear out the list, so you can also call this
   * when a tree is detached to clear the list.
   *
   * This does NOT update the screen.
   */
  _buildVisibleList: function PTV__buildVisibleList() {
    if (this._result) {
      // Any current visible elements need to be marked as invisible.
      for (var i = 0; i < this._visibleElements.length; i++) {
        this._visibleElements[i].viewIndex = -1;
      }
    }

    var oldCount = this.rowCount;
    this._visibleElements.splice(0);
    if (this._tree)
      this._tree.rowCountChanged(0, -oldCount);

    var rootNode = this._result.root;
    if (rootNode && this._tree) {
      this._computeShowSessions();

      asContainer(rootNode);
      if (this._showRoot) {
        // List the root node
        this._visibleElements.push(this._result.root);
        this._tree.rowCountChanged(0, 1);
        this._result.root.viewIndex = 0;
      }
      else if (!rootNode.containerOpen) {
        // this triggers containerOpened which then builds the visible
        // selection
        rootNode.containerOpen = true;
        return;
      }

      this.invalidateContainer(rootNode);
    }
  },

  /**
   * This takes a container and recursively appends visible elements to the
   * given array. This is used to build the visible element list (with
   * this._visibleElements passed as the array), or portions thereof (with
   * a separate array that is merged with the main list later.
   *
   * aVisibleStartIndex is the visible index of the beginning of the 'aVisible'
   * array. When aVisible is this._visibleElements, this is 0. This is non-zero
   * when we are building up a sub-region for insertion. Then, this is the
   * index where the new array will be inserted into this._visibleElements.
   * It is used to compute each node's viewIndex.
   */
  _buildVisibleSection:
  function PTV__buildVisibleSection(aContainer, aVisible, aVisibleStartIndex)
  {
    if (!aContainer.containerOpen)
      return;  // nothing to do

    var cc = aContainer.childCount;
    for (var i=0; i < cc; i++) {
      var curChild = aContainer.getChild(i);

      // collapse all duplicates starting from here
      if (this._collapseDuplicates) {
        var showThis = { value: false };
        while (i <  cc - 1 &&
               this._canCollapseDuplicates(curChild, aContainer.getChild(i+1),
                                           showThis)) {
          if (showThis.value) {
            // collapse the first and use the second
            curChild.viewIndex = -1;
            curChild = aContainer.getChild(i+1);
          }
          else {
            // collapse the second and use the first
            aContainer.getChild(i+1).viewIndex = -1;
          }
          i++;
        }
      }

      // don't display separators when sorted
      if (PlacesUtils.nodeIsSeparator(curChild)) {
        if (this._result.sortingMode !=
            Ci.nsINavHistoryQueryOptions.SORT_BY_NONE) {
          curChild.viewIndex = -1;
          continue;
        }
      }

      // add item
      curChild.viewIndex = aVisibleStartIndex + aVisible.length;
      aVisible.push(curChild);

      // recursively do containers
      if (PlacesUtils.nodeIsContainer(curChild)) {
        asContainer(curChild);
        if (curChild.containerOpen && curChild.childCount > 0)
          this._buildVisibleSection(curChild, aVisible, aVisibleStartIndex);
      }
    }
  },

  /**
   * This counts how many rows an item takes in the tree, that is, the
   * item itself plus any nodes following it with an increased indent.
   * This allows you to figure out how many rows an item (=1) or a
   * container with all of its children takes.
   */
  _countVisibleRowsForItem: function PTV__countVisibleRowsForItem(aNode) {
    if (aNode == this._result.root)
      return this._visibleElements.length;

    var viewIndex = aNode.viewIndex;
    NS_ASSERT(viewIndex >= 0, "Item is not visible, no rows to count");
    var outerLevel = aNode.indentLevel;
    for (var i = viewIndex + 1; i < this._visibleElements.length; i++) {
      if (this._visibleElements[i].indentLevel <= outerLevel)
        return i - viewIndex;
    }
    // this node plus its children occupy the bottom of the list
    return this._visibleElements.length - viewIndex;
  },

  /**
   * This is called by containers when they change and we need to update
   * everything about the container. We build a new visible section with
   * the container as a separate object so we first know how the list
   * changes. This way we only have to do one realloc/memcpy to update
   * the list.
   *
   * We also try to be smart here about redrawing the screen.
   */
  _refreshVisibleSection: function PTV__refreshVisibleSection(aContainer) {
    NS_ASSERT(this._result, "Need to have a result to update");
    if (!this._tree)
      return;

    // The root node is invisible if showRoot is not set. Otherwise aContainer
    // must be visible
    if (this._showRoot || aContainer != this._result.root) {
      if (aContainer.viewIndex < 0 &&
          aContainer.viewIndex > this._visibleElements.length)
        throw "Trying to expand a node that is not visible";

      NS_ASSERT(this._visibleElements[aContainer.viewIndex] == aContainer,
                "Visible index is out of sync!");
    }

    var startReplacement = aContainer.viewIndex + 1;
    var replaceCount = this._countVisibleRowsForItem(aContainer);

    // We don't replace the container item itself so we decrease the
    // replaceCount by 1. We don't do so though if there is no visible item
    // for the container. This happens when aContainer is the root node and
    // showRoot is not set.
    if (aContainer.viewIndex != -1)
      replaceCount-=1;

    // Mark the removees as invisible
    for (var i = startReplacement; i < replaceCount; i ++)
      this._visibleElements[startReplacement + i].viewIndex = -1;

    // Building the new list will set the new elements' visible indices.
    var newElements = [];
    this._buildVisibleSection(aContainer, newElements, startReplacement);

    // actually update the visible list
    this._visibleElements = 
      this._visibleElements.slice(0, startReplacement).concat(newElements)
          .concat(this._visibleElements.slice(startReplacement + replaceCount,
                                              this._visibleElements.length));

    if (replaceCount == newElements.length) {
      // length is the same, just repaint the changed area
      if (replaceCount > 0) {
        this._tree.invalidateRange(startReplacement,
                                   startReplacement + replaceCount - 1);
      }
    }
    else {
      // If the new area hasa different size, we'll have to renumber the
      // elements following the area.
      for (i = startReplacement + newElements.length;
           i < this._visibleElements.length; i ++) {
        this._visibleElements[i].viewIndex = i;
      }

      // repaint the region in which we didn't change the viewIndexes, also
      // including the container element itself since its twisty could have
      // changed.
      var minLength = Math.min(newElements.length, replaceCount);
      this._tree.invalidateRange(startReplacement - 1,
                                 startReplacement + minLength - 1);

      // now update the number of elements
      this._tree.rowCountChanged(startReplacement + minLength,
                                 newElements.length - replaceCount);
    }
  },

  /**
   * This returns true if the two results can be collapsed as duplicates.
   * aShowThisOne will be either 0 or 1, indicating which of the
   * duplicates should be shown.
   */
  _canCollapseDuplicates:
  function PTV__canCollapseDuplicate(aTop, aNext, aShowThisOne) {
    if (!this._collapseDuplicates)
      return false;
    if (!PlacesUtils.nodeIsVisit(aTop) ||
        !PlacesUtils.nodeIsVisit(aNext))
      return false; // only collapse two visits

    asVisit(aTop);
    asVisit(aNext);

    if (aTop.uri != aNext.uri)
      return false; // don't collapse nonmatching URIs

    // now we know we want to collapse, show the one with the more recent time
    aShowThisOne.value = aTop.time < aNext.time;
    return true;
  },

  COLUMN_TYPE_UNKNOWN: 0,
  COLUMN_TYPE_TITLE: 1,
  COLUMN_TYPE_URI: 2,
  COLUMN_TYPE_DATE: 3,
  COLUMN_TYPE_VISITCOUNT: 4,
  _getColumnType: function PTV__getColumnType(aColumn) {
    switch (aColumn.id) {
      case "title":
        return this.COLUMN_TYPE_TITLE;
      case "url":
        return this.COLUMN_TYPE_URI;
      case "date":
        return this.COLUMN_TYPE_DATE;
      case "visitCount":
        return this.COLUMN_TYPE_VISITCOUNT;
    }
    return this.COLUMN_TYPE_UNKNOWN;
  },

  _sortTypeToColumnType: function PTV__sortTypeToColumnType(aSortType) {
    switch(aSortType) {
      case Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING:
        return [this.COLUMN_TYPE_TITLE, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_DESCENDING:
        return [this.COLUMN_TYPE_TITLE, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING:
        return [this.COLUMN_TYPE_DATA, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING:
        return [this.COLUMN_TYPE_DATA, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_URI_ASCENDING:
        return [this.COLUMN_TYPE_URI, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_URI_DESCENDING:
        return [this.COLUMN_TYPE_URI, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_ASCENDING:
        return [this.COLUMN_TYPE_VISITCOUNT, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING:
        return [this.COLUMN_TYPE_VISITCOUNT, true];
    }
    return [this.COLUMN_TYPE_UNKNOWN, false];
  },

  // nsINavHistoryResultViewer
  itemInserted: function PTV_itemInserted(aParent, aItem, aNewIndex) {
    if (!this._tree)
      return;
    if (!this._result)
      throw Cr.NS_ERROR_UNEXPECTED;

    if (PlacesUtils.nodeIsSeparator(aItem) &&
        this._result.sortingMode != Ci.nsINavHistoryQueryOptions.SORT_BY_NONE) {
      aItem.viewIndex = -1;
      return;
    }

    // update parent when inserting the first item because twisty may
    // have changed
    if (aParent.childCount == 1)
      this.itemChanged(aParent);

    // compute the new view index of the item
    var newViewIndex = -1;
    if (aNewIndex == 0) {
      // item is the first thing in our child list, it takes our index +1. Note
      // that this computation still works if the parent is an invisible root
      // node, because root_index + 1 = -1 + 1 = 0
      newViewIndex = aParent.viewIndex + 1;
    }
    else {
      // Here, we try to find the next visible element in the child list so we
      // can set the new visible index to be right before that. Note that we
      // have to search DOWN instead of up, because some siblings could have
      // children themselves that would be in the way.
      for (var i = aNewIndex + 1; i < aParent.childCount; i ++) {
        var viewIndex = aParent.getChild(i).viewIndex;
        if (viewIndex >= 0) {
          // the view indices of subsequent children have not been shifted so
          // the next item will have what should be our index
          newViewIndex = viewIndex;
          break;
        }
      }
      if (newViewIndex < 0) {
        // At the end of the child list without finding a visible sibling: This
        // is a little harder because we don't know how many rows the last item
        // in our list takes up (it could be a container with many children).
        var lastRowCount =
          this._countVisibleRowsForItem(aParent.getChild(aNewIndex - 1));
        newViewIndex =
          aParent.getChild(aNewIndex - 1).viewIndex + lastRowCount;
      }
    }

    // Try collapsing with the previous node. Note that we do not have to try
    // to redraw the surrounding rows (which we normally would because session
    // boundaries may have changed) because when an item is merged, it will
    // always be in the same session.
    var showThis =  { value: true };
    if (newViewIndex > 0 &&
        this._canCollapseDuplicates
          (this._visibleElements[newViewIndex - 1], aItem, showThis)) {
      if (!showThis.value) {
        // new node is invisible, collapsed with previous one
        aItem.viewIndex = -1;
      }
      else {
        // new node replaces the previous
       this.itemReplaced(aParent, this._visibleElements[newViewIndex - 1],
                         aItem, 0);
      }
      return;
    }

    // try collapsing with the next node (which is currently at the same
    // index we are trying to insert at)
    if (newViewIndex < this._visibleElements.length &&
        this._canCollapseDuplicates(aItem, this._visibleElements[newViewIndex],
                                    showThis)) {
      if (!showThis.value) {
        // new node replaces the next node
        this.itemReplaced(aParent, this._visibleElements[newViewIndex], aItem,
                          0);
      }
      else {
        // new node is invisible, replaced by next one
        aItem.viewIndex = -1;
      }
      return;
    }

    // no collapsing, insert new item
    aItem.viewIndex = newViewIndex;
    this._visibleElements.splice(newViewIndex, 0, aItem);
    for (var i = newViewIndex + 1;
         i < this._visibleElements.length; i ++) {
      this._visibleElements[i].viewIndex = i;
    }
    this._tree.rowCountChanged(newViewIndex, 1);

    // Need to redraw the rows around this one because session boundaries
    // may have changed. For example, if we add a page to a session, the
    // previous page will need to be redrawn because it's session border
    // will disappear.
    if (this._showSessions) {
      if (newViewIndex > 0)
        this._tree.invalidateRange(newViewIndex - 1, newViewIndex - 1);
      if (newViewIndex < this._visibleElements.length -1)
        this._tree.invalidateRange(newViewIndex + 1, newViewIndex + 1);
    }

    if (PlacesUtils.nodeIsContainer(aItem) && asContainer(aItem).containerOpen)
      this._refreshVisibleSection(aItem);
  },

  /**
   * THIS FUNCTION DOES NOT HANDLE cases where a collapsed node is being
   * removed but the node it is collapsed with is not being removed (this then
   * just swap out the removee with it's collapsing partner). The only time
   * when we really remove things is when deleting URIs, which will apply to
   * all collapsees. This function is called sometimes when resorting items.
   * However, we won't do this when sorted by date because dates will never
   * change for visits, and date sorting is the only time things are collapsed.
   */
  itemRemoved: function PTV_itemRemoved(aParent, aItem, aOldIndex) {
    NS_ASSERT(this._result, "Got a notification but have no result!");
    if (!this._tree)
        return; // nothing to do

    var oldViewIndex = aItem.viewIndex;
    if (oldViewIndex < 0)
      return; // item was already invisible, nothing to do

    // this may have been a container, in which case it has a lot of rows
    var count = this._countVisibleRowsForItem(aItem);

    // We really want tail recursion here, since we sometimes do another
    // remove after this when duplicates are being collapsed. This loop
    // emulates that.
    while (true) {
      NS_ASSERT(oldViewIndex <= this._visibleElements.length,
                "Trying to remove invalid row");
      if (oldViewIndex > this._visibleElements.length)
        return;

      this._visibleElements.splice(oldViewIndex, count);
      for (var i = oldViewIndex; i < this._visibleElements.length; i++)
        this._visibleElements[i].viewIndex = i;

      this._tree.rowCountChanged(oldViewIndex, -count);

      // the removal might have put two things together that should be collapsed
      if (oldViewIndex > 0 &&
          oldViewIndex < this._visibleElements.length) {
        var showThisOne =  { value: true };
        if (this._canCollapseDuplicates
             (this._visibleElements[oldViewIndex - 1],
              this._visibleElements[oldViewIndex], showThisOne))
        {
          // Fake-tail-recurse to the beginning of this function to
          // remove the collapsed row. Note that we need to set the
          // visible index to -1 before looping because we can never
          // touch the row we're removing (callers may have already
          // destroyed it).
          oldViewIndex = oldViewIndex - 1 + (showThisOne.value ? 1 : 0);
          this._visibleElements[oldViewIndex].viewIndex = -1;
          count = 1; // next time remove one row
          continue;
        }
      }
      break; // normal path: just remove once
    }

    // redraw parent because twisty may have changed
    if (!aParent.hasChildren)
      this.itemChanged(aParent);
  },

  /**
   * Be careful, the parameter 'aIndex' here specifies the index in the parent
   * node of the item, not the visible index.
   *
   * This is called from the result when the item is replaced, but this object
   * calls this function internally also when duplicate collapsing changes. In
   * this case, aIndex will be 0, so we should be careful not to use the value.
   */
  itemReplaced:
  function PTV_itemReplaced(aParent, aOldItem, aNewItem, aIndexDoNotUse) {
    if (!this._tree)
      return;

    var viewIndex = aOldItem.viewIndex;
    aNewItem.viewIndex = viewIndex;
    if (viewIndex >= 0 &&
        viewIndex < this._visibleElements.length)
      this._visibleElements[viewIndex] = aNewItem;
    aOldItem.viewIndex = -1;
    this._tree.invalidateRow(viewIndex);
  },

  itemChanged: function PTV_itemChanged(aItem) {
    NS_ASSERT(this._result, "Got a notification but have no result!");
    var viewIndex = aItem.viewIndex;
    if (this._tree && viewIndex >= 0)
      this._tree.invalidateRow(viewIndex);
  },

  containerOpened: function PTV_containerOpened(aItem) {
    this.invalidateContainer(aItem);
  },

  containerClosed: function PTV_containerClosed(aItem) {
    this.invalidateContainer(aItem);
  },

  invalidateContainer: function PTV_invalidateContainer(aItem) {
    NS_ASSERT(this._result, "Got a notification but have no result!");
    if (!this._tree)
      return; // nothing to do, container is not visible
    var viewIndex = aItem.viewIndex;
    if (viewIndex >= this._visibleElements.length) {
      // be paranoid about visible indices since others can change it
      throw Cr.NS_ERROR_UNEXPECTED;
    }
    this._refreshVisibleSection(aItem);
  },

  invalidateAll: function PTV_invalidateAll() {
    NS_ASSERT(this._result, "Got message but don't have a result!");
    if (!this._tree)
      return;

    var oldRowCount = this._visibleElements.length;

    // update flat list to new contents
    this._buildVisibleList();

    // redraw the tree, inserting new items
    this._tree.rowCountChanged(0, this._visibleElements.length - oldRowCount);
    this._tree.invalidate();
  },

  sortingChanged: function PTV__sortingChanged(aSortingMode) {
    if (!this._tree || !this._result)
      return;

    var columns = this._tree.columns;

    // clear old sorting indicator
    var sortedColumn = columns.getSortedColumn();
    if (sortedColumn)
      sortedColumn.element.removeAttribute("sortDirection");

    // set new sorting indicator by looking through all columns for ours
    if (aSortingMode == Ci.nsINavHistoryQueryOptions.SORT_BY_NONE)
      return;
    var [desiredColumn, desiredIsDescending] =
      this._sortTypeToColumnType(aSortingMode);
    var colCount = columns.count;
    for (var i = 0; i < colCount; i ++) {
      var column = columns.getColumnAt(i);
      if (this._getColumnType(column) == desiredColumn) {
        // found our desired one, set
        if (desiredIsDescending)
          column.element.setAttribute("sortDirection", "descending");
        else
          column.element.setAttribute("sortDirection", "ascending");
        break;
      }
    }
  },

  get result() {
    return this._result;
  },

  set result(val) {
    this._result = val;
    this._finishInit();
    return val;
  },

  addViewObserver: function PTV_addViewObserver(aObserver, aWeak) {
    if (aWeak)
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    if (this._observers.indexOf(aObserver) == -1)
      this._observers.push(aObserver);
  },

  removeViewObserver: function PTV_removeViewObserver(aObserver) {
    var index = this._observers.indexOf(aObserver);
    if (index != -1)
      this._observers.splice(index, 1);
  },

  _enumerateObservers: function PTV__enumerateObservers(aFunctionName, aArgs) {
    for (var i=0; i < this._observers.length; i++) {
      // Don't bail out if one of the observer threw
      try {
        var obs = this._observers[i];
        obs[aFunctionName].apply(obs, aArgs);
      }
      catch (ex) { Components.reportError(ex); }
    }
  },

  // nsINavHistoryResultTreeViewer
  get collapseDuplicates() {
    return this._collapseDuplicates;
  },

  set collapseDuplicates(val) {
    if (this._collapseDuplicates == val)
      return; // no change;

    this._collapseDuplicates = val;
    if (this._tree && this._result)
      this.invalidateAll();

    return val;
  },

  get flatItemCount() {
    return this._visibleElements.length;
  },

  nodeForTreeIndex: function PTV_nodeForTreeIndex(aIndex) {
    if (aIndex > this._visibleElements.length)
      throw Cr.NS_ERROR_INVALID_ARG;

    return this._visibleElements[aIndex];
  },

  treeIndexForNode: function PTV_treeNodeForIndex(aNode) {
    var viewIndex = aNode.viewIndex;
    if (viewIndex < 0)
      return Ci.nsINavHistoryResultTreeViewer.INDEX_INVISIBLE;

    NS_ASSERT(this._visibleElements[viewIndex] == aNode,
              "Node's visible index and array out of sync");
    return viewIndex;
  },

  // nsITreeView
  get rowCount() {
    return this._visibleElements.length;
  },

  get selection() {
    return this._selection;
  },

  set selection(val) {
    return this._selection = val;
  },

  getRowProperties: function PTV_getRowProperties(aRow, aProperties) {
    this._ensureValidRow(aRow);
    var node = this._visibleElements[aRow];

    // Add the container property if it's applicable for this row.
    if (PlacesUtils.nodeIsContainer(node))
      aProperties.AppendElement(this._containerAtom);

    // Next handle properties for session information.
    if (!this._showSessions)
      return;

    switch(this._getRowSessionStatus(aRow)) {
      case this.SESSION_STATUS_NONE:
        break;
      case this.SESSION_STATUS_START:
        aProperties.AppendElement(this._sessionStartAtom);
        break;
      case this.SESSION_STATUS_CONTINUE:
        aProperties.AppendElement(this._sessionContinueAtom);
        break
    }
  },

  getCellProperties: function(aRow, aColumn, aProperties) { },
  getColumnProperties: function(aColumn, aProperties) { },

  isContainer: function PTV_isContainer(aRow) {
    this._ensureValidRow(aRow);
    var node = this._visibleElements[aRow];
    if (PlacesUtils.nodeIsContainer(node)) {
      // treat non-expandable queries as non-containers
      if (PlacesUtils.nodeIsQuery(node)) {
        asQuery(node);
        if (node.queryOptions.expandQueries)
          return true;
        // the root node is always expandable
        if (!node.parent)
          return true;

        return false;
      }
      return true;
    }
    return false;
  },

  isContainerOpen: function PTV_isContainerOpen(aRow) {
    this._ensureValidRow(aRow);
    if (!PlacesUtils.nodeIsContainer(this._visibleElements[aRow]))
      throw Cr.NS_ERROR_INVALID_ARG;

    return asContainer(this._visibleElements[aRow]).containerOpen;
  },

  isContainerEmpty: function PTV_isContainerEmpty(aRow) {
    this._ensureValidRow(aRow);
    if (!PlacesUtils.nodeIsContainer(this._visibleElements[aRow]))
      throw Cr.NS_ERROR_INVALID_ARG;

    return !asContainer(this._visibleElements[aRow]).hasChildren;
  },

  isSeparator: function PTV_isSeparator(aRow) {
    this._ensureValidRow(aRow);
    return PlacesUtils.nodeIsSeparator(this._visibleElements[aRow]);
  },

  isSorted: function PTV_isSorted() {
    return this._result.sortingMode !=
           Components.interfaces.nsINavHistoryQueryOptions.SORT_BY_NONE;
  },

  canDrop: function PTV_canDrop(aRow, aOrientation) {
    for (var i=0; i < this._observers.length; i++) {
      if (this._observers[i].canDrop(aRow, aOrientation))
        return true;
    }
    return false;
  },

  drop: function PTV_drop(aRow, aOrientation) {
    this._enumerateObservers("onDrop", [aRow, aOrientation]);
  },

  getParentIndex: function PTV_getParentIndex(aRow) {
    this._ensureValidRow(aRow);
    var parent = this._visibleElements[aRow].parent;
    if (!parent || parent.viewIndex < 0)
      return -1;

    return parent.viewIndex;
  },

  hasNextSibling: function PTV_hasNextSibling(aRow, aAfterIndex) {
    this._ensureValidRow(aRow);
    if (aRow == this._visibleElements.length -1) {
      // this is the last thing in the list -> no next sibling
      return false;
    }

    return this._visibleElements[aRow].indentLevel ==
           this._visibleElements[aRow + 1].indentLevel;
  },

  getLevel: function PTV_getLevel(aRow) {
    this._ensureValidRow(aRow);

    // Level is 0 for items at the root level, 1 for its children and so on.
    // If we don't show the result's root node, the level is simply the node's
    // indentLevel; if we do, it is the node's indentLevel increased by 1.
    // That is because nsNavHistoryResult uses -1 as the indent level for the
    // root node regardless of our internal showRoot state.
    if (this._showRoot)
      return this._visibleElements[aRow].indentLevel + 1;

    return this._visibleElements[aRow].indentLevel;
  },

  getImageSrc: function PTV_getImageSrc(aRow, aColumn) {
    this._ensureValidRow(aRow);

    // only the first column has an image
    if (aColumn.index != 0)
      return "";

    var node = this._visibleElements[aRow];

    // Containers may or may not have favicons. If not, we will return
    // nothing as the image, and the style rule should pick up the
    // default. Separator rows never have icons.
    if (PlacesUtils.nodeIsSeparator(node) ||
        (PlacesUtils.nodeIsContainer(node) && !node.icon))
      return "";

    // For consistency, we always return a favicon for non-containers,
    // even if it is just the default one.
    var icon = node.icon || PlacesUtils.favicons.defaultFavicon;
    if (icon)
      return icon.spec;
  },

  getProgressMode: function(aRow, aColumn) { },
  getCellValue: function(aRow, aColumn) { },

  getCellText: function PTV_getCellText(aRow, aColumn) {
    this._ensureValidRow(aRow);

    var node = this._visibleElements[aRow];
    switch (this._getColumnType(aColumn)) {
      case this.COLUMN_TYPE_TITLE:
        // normally, this is just the title, but we don't want empty items in
        // the tree view so return a special string if the title is empty.
        // Do it here so that callers can still get at the 0 length title
        // if they go through the "result" API.
        if (PlacesUtils.nodeIsSeparator(node))
          return "";
        return node.title || PlacesUtils.getString("noTitle");
      case this.COLUMN_TYPE_URI:
        if (PlacesUtils.nodeIsURI(node))
          return node.uri;

        return "";
      case this.COLUMN_TYPE_DATE:
        if (node.time == 0 || !PlacesUtils.nodeIsURI(node)) {
          // hosts and days shouldn't have a value for the date column.
          // Actually, you could argue this point, but looking at the
          // results, seeing the most recently visited date is not what
          // I expect, and gives me no information I know how to use.
          // Only show this for URI-based items.
          return "";
        }
        if (this._getRowSessionStatus(aRow) != this.SESSION_STATUS_CONTINUE) {
          var nodeTime = node.time / 1000; // PRTime is in microseconds
          var nodeTimeObj = new Date(nodeTime);

          // Check if it is today and only display the time.  Only bother
          // checking for today if it's within the last 24 hours, since
          // computing midnight is not really cheap. Sometimes we may get dates
          // in the future, so always show those.
          var ago = new Date(Date.now() - nodeTime);
          var dateFormat = Ci.nsIScriptableDateFormat.dateFormatShort;
          if (ago > -10000 && ago < (1000 * 24 * 60 * 60)) {
            var midnight = new Date(nodeTime);
            midnight.setHours(0);
            midnight.setMinutes(0);
            midnight.setSeconds(0);
            midnight.setMilliseconds(0);

            if (nodeTime > midnight.getTime())
              dateFormat = Ci.nsIScriptableDateFormat.dateFormatNone;
          }

          return (this._dateService.FormatDateTime("", dateFormat,
            Ci.nsIScriptableDateFormat.timeFormatNoSeconds,
            nodeTimeObj.getFullYear(), nodeTimeObj.getMonth() + 1,
            nodeTimeObj.getDate(), nodeTimeObj.getHours(),
            nodeTimeObj.getMinutes(), nodeTimeObj.getSeconds()));
        }
        return "";
      case this.COLUMN_TYPE_VISITCOUNT:
        return node.accessCount;
    }
    return "";
  },

  setTree: function PTV_setTree(aTree) {
    var hasOldTree = this._tree != null;
    this._tree = aTree;

    // do this before detaching from result when there is no tree.
    // This ensures that the visible indices of the elements in the
    // result have been set to -1
    this._finishInit();

    if (!aTree && hasOldTree && this._result) {
      // detach from result when we are detaching from the tree.
      // This breaks the reference cycle between us and the result.
      this._result.viewer = null;
    }
  },

  toggleOpenState: function PTV_toggleOpenState(aRow) {
    if (!this._result)
      throw Cr.NS_ERROR_UNEXPECTED;
    this._ensureValidRow(aRow);

    this._enumerateObservers("onToggleOpenState", [aRow]);

    var node = this._visibleElements[aRow];
    if (!PlacesUtils.nodeIsContainer(node))
      return; // not a container, nothing to do

    asContainer(node);
    node.containerOpen = !node.containerOpen;
  },

  cycleHeader: function PTV_cycleHeader(aColumn) {
    if (!this._result)
      throw Cr.NS_ERROR_UNEXPECTED;

    this._enumerateObservers("onCycleHeader", [aColumn]);

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
    var allowTriState = PlacesUtils.nodeIsFolder(this._result.root);

    var oldSort = this._result.sortingMode;
    var newSort;
    const NHQO = Ci.nsINavHistoryQueryOptions;
    switch (this._getColumnType(aColumn)) {
      case this.COLUMN_TYPE_TITLE:
        if (oldSort == NHQO.SORT_BY_TITLE_ASCENDING)
          newSort = NHQO.SORT_BY_TITLE_DESCENDING;
        else if (allowTriState && oldSort == NHQO.SORT_BY_TITLE_DESCENDING)
          newSort = NHQO.SORT_BY_NONE;
        else
          newSort = NHQO.SORT_BY_TITLE_ASCENDING;

        break;
      case this.COLUMN_TYPE_URI:
        if (oldSort == NHQO.SORT_BY_URI_ASCENDING)
          newSort = NHQO.SORT_BY_URI_DESCENDING;
        else if (allowTriState && oldSort == NHQO.SORT_BY_URI_DESCENDING)
          newSort = NHQO.SORT_BY_NONE;
        else
          newSort = NHQO.SORT_BY_URI_ASCENDING;

        break;
      case this.COLUMN_TYPE_DATE:
        if (oldSort == NHQO.SORT_BY_DATE_ASCENDING)
          newSort = NHQO.SORT_BY_DATE_DESCENDING;
        else if (allowTriState &&
                 oldSort == NHQO.SORT_BY_DATE_DESCENDING)
          newSort = NHQO.SORT_BY_NONE;
        else
          newSort = NHQO.SORT_BY_DATE_ASCENDING;

        break;
      case this.COLUMN_TYPE_VISITCOUNT:
        // visit count default is unusual because we sort by descending
        // by default because you are most likely to be looking for
        // highly visited sites when you click it
        if (oldSort == NHQO.SORT_BY_VISITCOUNT_DESCENDING)
          newSort = NHQO.SORT_BY_VISITCOUNT_ASCENDING;
        else if (allowTriState && oldSort == NHQO.SORT_BY_VISITCOUNT_ASCENDING)
          newSort = NHQO.SORT_BY_NONE;
        else
          newSort = NHQO.SORT_BY_VISITCOUNT_DESCENDING;

        break;
      default:
        throw Cr.NS_ERROR_INVALID_ARG;
    }
    this._result.sortingMode = newSort;
  },

  selectionChanged: function PTV_selectionChnaged() {
    this._enumerateObservers("onSelectionChanged");
  },

  cycleCell: function PTV_cycleCell(aRow, aColumn) {
    this._enumerateObservers("onCycleCell", [aRow, aColumn]);
  },

  isEditable: function(aRow, aColumn) { return false; },
  isSelectable: function(aRow, aColumn) { return false; },
  setCellText: function(aRow, aColumn) { },

  performAction: function PTV_performAction(aAction) {
    this._enumerateObservers("onPerformAction", [aAction]);
  },

  performActionOnRow: function PTV_perfromActionOnRow(aAction, aRow) {
    this._enumerateObservers("onPerformActionOnRow", [aAction, aRow]);
  },

  performActionOnCell:
  function PTV_performActionOnCell(aAction, aRow, aColumn) {
    this._enumerateObservers("onPerformActionOnRow", [aAction, aRow, aColumn]);
  }
};

function PlacesTreeView(aShowRoot) {
  this._tree = null;
  this._result = null;
  this._collapseDuplicates = true;
  this._showSessions = false;
  this._selection = null;
  this._visibleElements = [];
  this._observers = [];
  this._showRoot = aShowRoot;
}
