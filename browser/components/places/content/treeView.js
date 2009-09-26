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

  _atoms: [],
  _getAtomFor: function PTV__getAtomFor(aName) {
    if (!this._atoms[aName])
      this._atoms[aName] = this._makeAtom(aName);

    return this._atoms[aName];
  },

  _ensureValidRow: function PTV__ensureValidRow(aRow) {
    if (aRow < 0 || aRow >= this._visibleElements.length)
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
   * This is called once both the result and the tree are set.
   */
  _finishInit: function PTV__finishInit() {
    var selection = this.selection;
    if (selection)
      selection.selectEventsSuppressed = true;

    this._rootNode._viewIndex = -1;
    if (!this._rootNode.containerOpen) {
      // This triggers containerOpened which then builds the visible section.
      this._rootNode.containerOpen = true;
    }
    else
      this.invalidateContainer(this._rootNode);

    // "Activate" the sorting column and update commands.
    this.sortingChanged(this._result.sortingMode);

    if (selection)
      selection.selectEventsSuppressed = false;
  },

  _rootNode: null,

  /**
   * This takes a container and recursively appends visible elements to the
   * given array. This is used to build the visible element list (with
   * this._visibleElements passed as the array), or portions thereof (with
   * a separate array that is merged with the main list later).
   *
   * aVisibleStartIndex is the visible index of the beginning of the 'aVisible'
   * array. When aVisible is this._visibleElements, this is 0. This is non-zero
   * when we are building up a sub-region for insertion. Then, this is the
   * index where the new array will be inserted into this._visibleElements.
   * It is used to compute each node's viewIndex.
   */
  _buildVisibleSection:
  function PTV__buildVisibleSection(aContainer, aVisible, aToOpen, aVisibleStartIndex)
  {
    if (!aContainer.containerOpen)
      return;  // nothing to do

    const openLiteral = PlacesUIUtils.RDF.GetResource("http://home.netscape.com/NC-rdf#open");
    const trueLiteral = PlacesUIUtils.RDF.GetLiteral("true");

    var cc = aContainer.childCount;
    var sortingMode = this._result.sortingMode;
    for (var i=0; i < cc; i++) {
      var curChild = aContainer.getChild(i);
      var curChildType = curChild.type;

      // Don't display separators when sorted.
      if (curChildType == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR) {
        if (sortingMode != Ci.nsINavHistoryQueryOptions.SORT_BY_NONE) {
          curChild._viewIndex = -1;
          continue;
        }
      }

      // Add the node to the visible-nodes array and set its viewIndex.
      curChild._viewIndex = aVisibleStartIndex + aVisible.length;
      aVisible.push(curChild);

      // Recursively do containers.
      if (!this._flatList &&
          curChild instanceof Ci.nsINavHistoryContainerResultNode) {
        var resource = this._getResourceForNode(curChild);
        var isopen = resource != null &&
                     PlacesUIUtils.localStore.HasAssertion(resource, openLiteral,
                                                           trueLiteral, true);
        if (isopen != curChild.containerOpen)
          aToOpen.push(curChild);
        else if (curChild.containerOpen && curChild.childCount > 0)
          this._buildVisibleSection(curChild, aVisible, aToOpen, aVisibleStartIndex);
      }
    }
  },

  /**
   * This counts how many rows a node takes in the tree.  For containers it
   * will count the node itself plus any child node following it.
   */
  _countVisibleRowsForNode: function PTV__countVisibleRowsForNode(aNode) {
    if (aNode == this._rootNode)
      return this._visibleElements.length;

    var viewIndex = aNode._viewIndex;
    NS_ASSERT(viewIndex >= 0, "Node is not visible, no rows to count");
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
   * changes.
   *
   * We also try to be smart here about redrawing the screen.
   */
  _refreshVisibleSection: function PTV__refreshVisibleSection(aContainer) {
    NS_ASSERT(this._result, "Need to have a result to update");
    if (!this._tree)
      return;

    // The root node is invisible.
    if (aContainer != this._rootNode) {
      if (aContainer._viewIndex < 0 ||
          aContainer._viewIndex > this._visibleElements.length)
        throw "Trying to expand a node that is not visible";

      NS_ASSERT(this._visibleElements[aContainer._viewIndex] == aContainer,
                "Visible index is out of sync!");
    }

    var startReplacement = aContainer._viewIndex + 1;
    var replaceCount = this._countVisibleRowsForNode(aContainer);

    // We don't replace the container node itself so we should decrease the
    // replaceCount by 1, unless the container is our root node, which isn't
    // visible.
    if (aContainer != this._rootNode)
      replaceCount -= 1;

    // Persist selection state.
    var previouslySelectedNodes = [];
    var selection = this.selection;
    var rc = selection.getRangeCount();
    for (var rangeIndex = 0; rangeIndex < rc; rangeIndex++) {
      var min = { }, max = { };
      selection.getRangeAt(rangeIndex, min, max);
      var lastIndex = Math.min(max.value, startReplacement + replaceCount -1);
      // If this range does not overlap the replaced chunk, we don't need to
      // persist the selection.
      if (max.value < startReplacement || min.value > lastIndex)
        continue;

      // If this range starts before the replaced chunk, we should persist from
      // startReplacement to lastIndex.
      var firstIndex = Math.max(min.value, startReplacement);
      for (var nodeIndex = firstIndex; nodeIndex <= lastIndex; nodeIndex++) {
        // Mark the node invisible if we're about to remove it,
        // otherwise we'll try to select it later.
        var node = this._visibleElements[nodeIndex];
        if (nodeIndex >= startReplacement &&
            nodeIndex < startReplacement + replaceCount)
          node._viewIndex = -1;

        previouslySelectedNodes.push(
          { node: node, oldIndex: nodeIndex });
      }
    }

    // Building the new list will set the new elements' visible indices.
    var newElements = [];
    var toOpenElements = [];
    this._buildVisibleSection(aContainer,
                              newElements, toOpenElements, startReplacement);

    // Actually update the visible list.
    // XXX: We can probably make this more efficient using splice through
    // Function.apply.
    this._visibleElements =
      this._visibleElements.slice(0, startReplacement).concat(newElements)
          .concat(this._visibleElements.slice(startReplacement + replaceCount,
                                              this._visibleElements.length));

    // If the new area has a different size, we'll have to renumber the
    // elements following the area.
    if (replaceCount != newElements.length) {
      for (var i = startReplacement + newElements.length;
           i < this._visibleElements.length; i++) {
        this._visibleElements[i]._viewIndex = i;
      }
    }

    // now update the number of elements
    selection.selectEventsSuppressed = true;
    this._tree.beginUpdateBatch();

    if (replaceCount)
      this._tree.rowCountChanged(startReplacement, -replaceCount);
    if (newElements.length)
      this._tree.rowCountChanged(startReplacement, newElements.length);

    if (!this._flatList) {
      // now, open any containers that were persisted
      for (var i = 0; i < toOpenElements.length; i++) {
        var item = toOpenElements[i];
        var parent = item.parent;
        // avoid recursively opening containers
        while (parent) {
          if (parent.uri == item.uri)
            break;
          parent = parent.parent;
        }
        // if we don't have a parent, we made it all the way to the root
        // and didn't find a match, so we can open our item
        if (!parent && !item.containerOpen)
          item.containerOpen = true;
      }
    }

    this._tree.endUpdateBatch();

    // restore selection
    if (previouslySelectedNodes.length > 0) {
      for (var i = 0; i < previouslySelectedNodes.length; i++) {
        var nodeInfo = previouslySelectedNodes[i];
        var index = nodeInfo.node._viewIndex;

        // If the nodes under the invalidated container were preserved, we can
        // just use viewIndex.
        if (index == -1) {
          // Otherwise, try to find an equal node.
          var itemId = PlacesUtils.getConcreteItemId(nodeInfo.node);
          if (itemId != 1) {
            // Search by itemId.
            for (var j = 0; j < newElements.length && index == -1; j++) {
              if (PlacesUtils.getConcreteItemId(newElements[j]) == itemId)
                index = newElements[j]._viewIndex;
            }
          }
          else {
            // Search by uri.
            var uri = nodeInfo.node.uri;
            if (uri) {
              for (var j = 0; j < newElements.length && index == -1; j++) {
                if (newElements[j].uri == uri)
                  index = newElements[j]._viewIndex;
              }
            }
          }
        }

        // Select the found node, if any.
        if (index != -1)
          selection.rangedSelect(index, index, true);
      }

      // If only one node was previously selected and there's no selection now,
      // select the node at its old viewIndex, if any.
      if (previouslySelectedNodes.length == 1 &&
          selection.getRangeCount() == 0 &&
          this._visibleElements.length > previouslySelectedNodes[0].oldIndex) {
        selection.rangedSelect(previouslySelectedNodes[0].oldIndex,
                               previouslySelectedNodes[0].oldIndex, true);
      }
    }
    selection.selectEventsSuppressed = false;
  },

  _convertPRTimeToString: function PTV__convertPRTimeToString(aTime) {
    var timeInMilliseconds = aTime / 1000; // PRTime is in microseconds

    // Date is calculated starting from midnight, so the modulo with a day are
    // milliseconds from today's midnight.
    // getTimezoneOffset corrects that based on local time.
    // 86400000 = 24 * 60 * 60 * 1000 = 1 day
    // 60000 = 60 * 1000 = 1 minute
    var dateObj = new Date();
    var timeZoneOffsetInMs = dateObj.getTimezoneOffset() * 60000;
    var now = dateObj.getTime() - timeZoneOffsetInMs;
    var midnight = now - (now % (86400000));

    var dateFormat = timeInMilliseconds - timeZoneOffsetInMs >= midnight ?
                      Ci.nsIScriptableDateFormat.dateFormatNone :
                      Ci.nsIScriptableDateFormat.dateFormatShort;

    var timeObj = new Date(timeInMilliseconds);
    return (this._dateService.FormatDateTime("", dateFormat,
      Ci.nsIScriptableDateFormat.timeFormatNoSeconds,
      timeObj.getFullYear(), timeObj.getMonth() + 1,
      timeObj.getDate(), timeObj.getHours(),
      timeObj.getMinutes(), timeObj.getSeconds()));
  },

  COLUMN_TYPE_UNKNOWN: 0,
  COLUMN_TYPE_TITLE: 1,
  COLUMN_TYPE_URI: 2,
  COLUMN_TYPE_DATE: 3,
  COLUMN_TYPE_VISITCOUNT: 4,
  COLUMN_TYPE_KEYWORD: 5,
  COLUMN_TYPE_DESCRIPTION: 6,
  COLUMN_TYPE_DATEADDED: 7,
  COLUMN_TYPE_LASTMODIFIED: 8,
  COLUMN_TYPE_TAGS: 9,

  _getColumnType: function PTV__getColumnType(aColumn) {
    var columnType = aColumn.element.getAttribute("anonid") || aColumn.id;

    switch (columnType) {
      case "title":
        return this.COLUMN_TYPE_TITLE;
      case "url":
        return this.COLUMN_TYPE_URI;
      case "date":
        return this.COLUMN_TYPE_DATE;
      case "visitCount":
        return this.COLUMN_TYPE_VISITCOUNT;
      case "keyword":
        return this.COLUMN_TYPE_KEYWORD;
      case "description":
        return this.COLUMN_TYPE_DESCRIPTION;
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
      case Ci.nsINavHistoryQueryOptions.SORT_BY_KEYWORD_ASCENDING:
        return [this.COLUMN_TYPE_KEYWORD, false];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_KEYWORD_DESCENDING:
        return [this.COLUMN_TYPE_KEYWORD, true];
      case Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_ASCENDING:
        if (this._result.sortingAnnotation == DESCRIPTION_ANNO)
          return [this.COLUMN_TYPE_DESCRIPTION, false];
        break;
      case Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_DESCENDING:
        if (this._result.sortingAnnotation == DESCRIPTION_ANNO)
          return [this.COLUMN_TYPE_DESCRIPTION, true];
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

  // nsINavHistoryResultViewer
  nodeInserted: function PTV_nodeInserted(aParentNode, aNode, aNewIndex) {
    if (!this._tree)
      return;
    if (!this._result)
      throw Cr.NS_ERROR_UNEXPECTED;

    if (PlacesUtils.nodeIsSeparator(aNode) &&
        this._result.sortingMode != Ci.nsINavHistoryQueryOptions.SORT_BY_NONE) {
      aNode._viewIndex = -1;
      return;
    }

    // Update parent when inserting the first item, since twisty may
    // have changed.
    if (aParentNode.childCount == 1)
      this._tree.invalidateRow(aParentNode._viewIndex);

    // compute the new view index of the item
    var newViewIndex = -1;
    if (aNewIndex == 0) {
      // item is the first thing in our child list, it takes our index +1. Note
      // that this computation still works if the parent is an invisible root
      // node, because root_index + 1 = -1 + 1 = 0
      newViewIndex = aParentNode._viewIndex + 1;
    }
    else {
      // Here, we try to find the next visible element in the child list so we
      // can set the new visible index to be right before that. Note that we
      // have to search DOWN instead of up, because some siblings could have
      // children themselves that would be in the way.
      for (var i = aNewIndex + 1; i < aParentNode.childCount; i++) {
        var viewIndex = aParentNode.getChild(i)._viewIndex;
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
        var prevChild = aParentNode.getChild(aNewIndex - 1);
        newViewIndex = prevChild._viewIndex + this._countVisibleRowsForNode(prevChild);
      }
    }

    aNode._viewIndex = newViewIndex;
    this._visibleElements.splice(newViewIndex, 0, aNode);
    for (var i = newViewIndex + 1;
         i < this._visibleElements.length; i++) {
      this._visibleElements[i]._viewIndex = i;
    }
    this._tree.rowCountChanged(newViewIndex, 1);

    if (PlacesUtils.nodeIsContainer(aNode) && asContainer(aNode).containerOpen)
      this._refreshVisibleSection(aNode);
  },

  // This is used in nodeRemoved and nodeMoved to fix viewIndex values.
  // Throws if the node has an invalid viewIndex.
  _fixViewIndexOnRemove: function PTV_fixViewIndexOnRemove(aNode,
                                                           aParentNode) {
    var oldViewIndex = aNode._viewIndex;
    // this may have been a container, in which case it has a lot of rows
    var count = this._countVisibleRowsForNode(aNode);

    if (oldViewIndex > this._visibleElements.length)
      throw("Trying to remove a node with an invalid viewIndex");

    this._visibleElements.splice(oldViewIndex, count);
    for (var i = oldViewIndex; i < this._visibleElements.length; i++)
      this._visibleElements[i]._viewIndex = i;

    this._tree.rowCountChanged(oldViewIndex, -count);

    // redraw parent because twisty may have changed
    if (!aParentNode.hasChildren)
      this._tree.invalidateRow(aParentNode._viewIndex);
  },

  /**
   * THIS FUNCTION DOES NOT HANDLE cases where a collapsed node is being
   * removed but the node it is collapsed with is not being removed (this then
   * just swap out the removee with its collapsing partner). The only time
   * when we really remove things is when deleting URIs, which will apply to
   * all collapsees. This function is called sometimes when resorting items.
   * However, we won't do this when sorted by date because dates will never
   * change for visits, and date sorting is the only time things are collapsed.
   */
  nodeRemoved: function PTV_nodeRemoved(aParentNode, aNode, aOldIndex) {
    NS_ASSERT(this._result, "Got a notification but have no result!");
    if (!this._tree)
      return; // nothing to do

    var oldViewIndex = aNode._viewIndex;
    if (oldViewIndex < 0) {
      // There's nothing to do if the node was already invisible.
      return;
    }

    // If the node was exclusively selected, the node next to it will be
    // selected.
    var selectNext = false;
    var selection = this.selection;
    if (selection.getRangeCount() == 1) {
      var min = { }, max = { };
      selection.getRangeAt(0, min, max);
      if (min.value == max.value &&
          this.nodeForTreeIndex(min.value) == aNode)
        selectNext = true;
    }

    // Remove the node and fix viewIndex values.
    this._fixViewIndexOnRemove(aNode, aParentNode);

    // Restore selection if the node was exclusively selected.
    if (!selectNext)
      return;

    // Restore selection
    if (this._visibleElements.length > oldViewIndex)
      selection.rangedSelect(oldViewIndex, oldViewIndex, true);    
    else if (this._visibleElements.length > 0) {
      // if we removed the last child, we select the new last child if exists
      selection.rangedSelect(this._visibleElements.length - 1,
                             this._visibleElements.length - 1, true);
    }
  },

  /**
   * Be careful, aOldIndex and aNewIndex specify the index in the
   * corresponding parent nodes, not the visible indexes.
   */
  nodeMoved:
  function PTV_nodeMoved(aNode, aOldParent, aOldIndex, aNewParent, aNewIndex) {
    NS_ASSERT(this._result, "Got a notification but have no result!");
    if (!this._tree)
      return; // nothing to do

    var oldViewIndex = aNode._viewIndex;
    if (oldViewIndex < 0) {
      // There's nothing to do if the node was already invisible.
      return;
    }

    // This may have been a container, in which case it has a lot of rows.
    var count = this._countVisibleRowsForNode(aNode);

    // Persist selection state.
    var nodesToSelect = [];
    var selection = this.selection;
    var rc = selection.getRangeCount();
    for (var rangeIndex = 0; rangeIndex < rc; rangeIndex++) {
      var min = { }, max = { };
      selection.getRangeAt(rangeIndex, min, max);
      var lastIndex = Math.min(max.value, oldViewIndex + count -1);
      if (min.value < oldViewIndex || min.value > lastIndex)
        continue;

      for (var nodeIndex = min.value; nodeIndex <= lastIndex; nodeIndex++)
        nodesToSelect.push(this._visibleElements[nodeIndex]);
    }
    if (nodesToSelect.length > 0)
      selection.selectEventsSuppressed = true;

    // Remove node from the old position.
    this._fixViewIndexOnRemove(aNode, aOldParent);

    // Insert the node into the new position.
    this.nodeInserted(aNewParent, aNode, aNewIndex);

    // Restore selection.
    if (nodesToSelect.length > 0) {
      for (var i = 0; i < nodesToSelect.length; i++) {
        var node = nodesToSelect[i];
        var index = node._viewIndex;
        selection.rangedSelect(index, index, true);
      }
      selection.selectEventsSuppressed = false;
    }
  },

  /**
   * Be careful, the parameter 'aIndex' here specifies the node's index in the
   * parent node, not the visible index.
   */
  nodeReplaced:
  function PTV_nodeReplaced(aParentNode, aOldNode, aNewNode, aIndexDoNotUse) {
    if (!this._tree)
      return;

    var viewIndex = aOldNode._viewIndex;
    aNewNode._viewIndex = viewIndex;
    if (viewIndex >= 0 &&
        viewIndex < this._visibleElements.length) {
      this._visibleElements[viewIndex] = aNewNode;
    }
    aOldNode._viewIndex = -1;
    this._tree.invalidateRow(viewIndex);
  },

  _invalidateCellValue: function PTV__invalidateCellValue(aNode,
                                                          aColumnType) {
    NS_ASSERT(this._result, "Got a notification but have no result!");
    let viewIndex = aNode._viewIndex;
    if (viewIndex == -1) // invisible
      return;

    if (this._tree) {
      let column = this._findColumnByType(aColumnType);
      if (column && !column.element.hidden)
        this._tree.invalidateCell(viewIndex, column);

      // Last modified time is altered for almost all node changes.
      if (aColumnType != this.COLUMN_TYPE_LASTMODIFIED) {
        let lastModifiedColumn =
          this._findColumnByType(this.COLUMN_TYPE_LASTMODIFIED);
        if (lastModifiedColumn && !lastModifiedColumn.hidden)
          this._tree.invalidateCell(viewIndex, lastModifiedColumn);
      }
    }
  },

  nodeTitleChanged: function PTV_nodeTitleChanged(aNode, aNewTitle) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_TITLE);
  },

  nodeURIChanged: function PTV_nodeURIChanged(aNode, aNewURI) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_URI);
  },

  nodeIconChanged: function PTV_nodeIconChanged(aNode) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_TITLE);
  },

  nodeHistoryDetailsChanged:
  function PTV_nodeHistoryDetailsChanged(aNode, aUpdatedVisitDate,
                                         aUpdatedVisitCount) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_DATE);
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_VISITCOUNT);
  },

  nodeTagsChanged: function PTV_nodeTagsChanged(aNode) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_TAGS);
  },

  nodeKeywordChanged: function PTV_nodeKeywordChanged(aNode, aNewKeyword) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_KEYWORD);
  },

  nodeAnnotationChanged: function PTV_nodeAnnotationChanged(aNode, aAnno) {
    if (aAnno == DESCRIPTION_ANNO)
      this._invalidateCellValue(aNode, this.COLUMN_TYPE_DESCRIPTION);
  },

  nodeDateAddedChanged: function PTV_nodeDateAddedChanged(aNode, aNewValue) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_DATEADDED);
  },

  nodeLastModifiedChanged:
  function PTV_nodeLastModifiedChanged(aNode, aNewValue) {
    this._invalidateCellValue(aNode, this.COLUMN_TYPE_LASTMODIFIED);
  },

  containerOpened: function PTV_containerOpened(aNode) {
    this.invalidateContainer(aNode);
  },

  containerClosed: function PTV_containerClosed(aNode) {
    this.invalidateContainer(aNode);
  },

  invalidateContainer: function PTV_invalidateContainer(aNode) {
    NS_ASSERT(this._result, "Got a notification but have no result!");
    if (!this._tree)
      return; // nothing to do, container is not visible

    if (aNode._viewIndex >= this._visibleElements.length) {
      // be paranoid about visible indices since others can change it
      throw Cr.NS_ERROR_UNEXPECTED;
    }
    this._refreshVisibleSection(aNode);
  },

  _columns: [],
  _findColumnByType: function PTV__findColumnByType(aColumnType) {
    if (this._columns[aColumnType])
      return this._columns[aColumnType];

    var columns = this._tree.columns;
    var colCount = columns.count;
    for (var i = 0; i < colCount; i++) {
      let column = columns.getColumnAt(i);
      let columnType = this._getColumnType(column);
      this._columns[columnType] = column;
      if (columnType == aColumnType)
        return column;
    }

    // That's completely valid.  Most of our trees actually include just the
    // title column.
    return null;
  },

  sortingChanged: function PTV__sortingChanged(aSortingMode) {
    if (!this._tree || !this._result)
      return;

    // depending on the sort mode, certain commands may be disabled
    window.updateCommands("sort");

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
    var column = this._findColumnByType(desiredColumn);
    if (column) {
      let sortDir = desiredIsDescending ? "descending" : "ascending";
      column.element.setAttribute("sortDirection", sortDir);
    }
  },

  get result() {
    return this._result;
  },

  set result(val) {
    // Some methods (e.g. getURLsFromContainer) temporarily null out the
    // viewer when they do temporary changes to the view, this does _not_
    // call setResult(null), but then, we're called again with the result
    // object which is already set for this viewer. At that point,
    // we should do nothing.
    if (this._result != val) {
      if (this._result)
        this._rootNode.containerOpen = false;

      this._result = val;
      this._rootNode = val ? val.root : null;

      // If the tree is not set yet, setTree will call finishInit.
      if (this._tree && val)
        this._finishInit();
    }
    return val;
  },

  nodeForTreeIndex: function PTV_nodeForTreeIndex(aIndex) {
    if (aIndex > this._visibleElements.length)
      throw Cr.NS_ERROR_INVALID_ARG;

    return this._visibleElements[aIndex];
  },

  treeIndexForNode: function PTV_treeNodeForIndex(aNode) {
    var viewIndex = aNode._viewIndex;
    if (viewIndex < 0)
      return Ci.nsINavHistoryResultTreeViewer.INDEX_INVISIBLE;

    NS_ASSERT(this._visibleElements[viewIndex] == aNode,
              "Node's visible index and array out of sync");
    return viewIndex;
  },

  _getResourceForNode: function PTV_getResourceForNode(aNode)
  {
    var uri = aNode.uri;
    NS_ASSERT(uri, "if there is no uri, we can't persist the open state");
    return uri ? PlacesUIUtils.RDF.GetResource(uri) : null;
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

  getRowProperties: function PTV_getRowProperties(aRow, aProperties) { },

  getCellProperties: function PTV_getCellProperties(aRow, aColumn, aProperties) {
    this._ensureValidRow(aRow);

    // for anonid-trees, we need to add the column-type manually
    var columnType = aColumn.element.getAttribute("anonid");
    if (columnType)
      aProperties.AppendElement(this._getAtomFor(columnType));
    else
      var columnType = aColumn.id;

    // Set the "ltr" property on url cells
    if (columnType == "url")
      aProperties.AppendElement(this._getAtomFor("ltr"));

    if (columnType != "title")
      return;

    var node = this._visibleElements[aRow];
    if (!node._cellProperties) {
      let properties = new Array();
      var itemId = node.itemId;
      var nodeType = node.type;
      if (PlacesUtils.containerTypes.indexOf(nodeType) != -1) {
        if (nodeType == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY) {
          properties.push(this._getAtomFor("query"));
          if (PlacesUtils.nodeIsTagQuery(node))
            properties.push(this._getAtomFor("tagContainer"));
          else if (PlacesUtils.nodeIsDay(node))
            properties.push(this._getAtomFor("dayContainer"));
          else if (PlacesUtils.nodeIsHost(node))
            properties.push(this._getAtomFor("hostContainer"));
        }
        else if (nodeType == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER ||
                 nodeType == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT) {
          if (PlacesUtils.nodeIsLivemarkContainer(node))
            properties.push(this._getAtomFor("livemark"));
        }

        if (itemId != -1) {
          var queryName = PlacesUIUtils.getLeftPaneQueryNameFromId(itemId);
          if (queryName)
            properties.push(this._getAtomFor("OrganizerQuery_" + queryName));
        }
      }
      else if (nodeType == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR)
        properties.push(this._getAtomFor("separator"));
      else if (PlacesUtils.nodeIsURI(node)) {
        properties.push(this._getAtomFor(PlacesUIUtils.guessUrlSchemeForUI(node.uri)));
        if (itemId != -1) {
          if (PlacesUtils.nodeIsLivemarkContainer(node.parent))
            properties.push(this._getAtomFor("livemarkItem"));
        }
      }

      node._cellProperties = properties;
    }
    for (var i = 0; i < node._cellProperties.length; i++)
      aProperties.AppendElement(node._cellProperties[i]);
  },

  getColumnProperties: function(aColumn, aProperties) { },

  isContainer: function PTV_isContainer(aRow) {
    this._ensureValidRow(aRow);

    var node = this._visibleElements[aRow];
    if (PlacesUtils.nodeIsContainer(node)) {
      // Flat-lists may ignore expandQueries and other query options when
      // they are asked to open a container.
      if (this._flatList)
        return true;

      // treat non-expandable childless queries as non-containers
      if (PlacesUtils.nodeIsQuery(node)) {
        var parent = node.parent;
        if ((PlacesUtils.nodeIsQuery(parent) ||
             PlacesUtils.nodeIsFolder(parent)) &&
            !node.hasChildren)
          return asQuery(parent).queryOptions.expandQueries;
      }
      return true;
    }
    return false;
  },

  isContainerOpen: function PTV_isContainerOpen(aRow) {
    if (this._flatList)
      return false;

    this._ensureValidRow(aRow);
    return this._visibleElements[aRow].containerOpen;
  },

  isContainerEmpty: function PTV_isContainerEmpty(aRow) {
    if (this._flatList)
      return true;

    this._ensureValidRow(aRow);
    return !this._visibleElements[aRow].hasChildren;
  },

  isSeparator: function PTV_isSeparator(aRow) {
    this._ensureValidRow(aRow);
    return PlacesUtils.nodeIsSeparator(this._visibleElements[aRow]);
  },

  isSorted: function PTV_isSorted() {
    return this._result.sortingMode !=
           Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
  },

  canDrop: function PTV_canDrop(aRow, aOrientation) {
    if (!this._result)
      throw Cr.NS_ERROR_UNEXPECTED;

    // drop position into a sorted treeview would be wrong
    if (this.isSorted())
      return false;

    var ip = this._getInsertionPoint(aRow, aOrientation);
    return ip && PlacesControllerDragHelper.canDrop(ip);
  },

  _getInsertionPoint: function PTV__getInsertionPoint(index, orientation) {
    var container = this._result.root;
    var dropNearItemId = -1;
    // When there's no selection, assume the container is the container
    // the view is populated from (i.e. the result's itemId).
    if (index != -1) {
      var lastSelected = this.nodeForTreeIndex(index);
      if (this.isContainer(index) && orientation == Ci.nsITreeView.DROP_ON) {
        // If the last selected item is an open container, append _into_
        // it, rather than insert adjacent to it. 
        container = lastSelected;
        index = -1;
      }
      else if (lastSelected.containerOpen &&
               orientation == Ci.nsITreeView.DROP_AFTER &&
               lastSelected.hasChildren) {
        // If the last selected node is an open container and the user is
        // trying to drag into it as a first node, really insert into it.
        container = lastSelected;
        orientation = Ci.nsITreeView.DROP_ON;
        index = 0;
      }
      else {
        // Use the last-selected node's container unless the root node
        // is selected, in which case we use the root node itself as the
        // insertion point.
        container = lastSelected.parent || container;

        // avoid the potentially expensive call to getIndexOfNode() 
        // if we know this container doesn't allow insertion
        if (PlacesControllerDragHelper.disallowInsertion(container))
          return null;

        var queryOptions = asQuery(this._result.root).queryOptions;
        if (queryOptions.sortingMode !=
              Ci.nsINavHistoryQueryOptions.SORT_BY_NONE) {
          // If we are within a sorted view, insert at the ends
          index = -1;
        }
        else if (queryOptions.excludeItems ||
                 queryOptions.excludeQueries ||
                 queryOptions.excludeReadOnlyFolders) {
          // Some item may be invisible, insert near last selected one.
          // We don't replace index here to avoid requests to the db,
          // instead it will be calculated later by the controller.
          index = -1;
          dropNearItemId = lastSelected.itemId;
        }
        else {
          var lsi = PlacesUtils.getIndexOfNode(lastSelected);
          index = orientation == Ci.nsITreeView.DROP_BEFORE ? lsi : lsi + 1;
        }
      }
    }

    if (PlacesControllerDragHelper.disallowInsertion(container))
      return null;

    return new InsertionPoint(PlacesUtils.getConcreteItemId(container),
                              index, orientation,
                              PlacesUtils.nodeIsTagQuery(container),
                              dropNearItemId);
  },

  drop: function PTV_drop(aRow, aOrientation) {
    // We are responsible for translating the |index| and |orientation| 
    // parameters into a container id and index within the container, 
    // since this information is specific to the tree view.
    var ip = this._getInsertionPoint(aRow, aOrientation);
    if (!ip)
      return;
    PlacesControllerDragHelper.onDrop(ip);
  },

  getParentIndex: function PTV_getParentIndex(aRow) {
    this._ensureValidRow(aRow);
    var parent = this._visibleElements[aRow].parent;
    if (!parent || parent._viewIndex < 0)
      return -1;

    return parent._viewIndex;
  },

  hasNextSibling: function PTV_hasNextSibling(aRow, aAfterIndex) {
    this._ensureValidRow(aRow);
    if (aRow == this._visibleElements.length -1) {
      // this is the last thing in the list -> no next sibling
      return false;
    }

    var thisLevel = this._visibleElements[aRow].indentLevel;
    for (var i = aAfterIndex + 1; i < this._visibleElements.length; ++i) {
      var nextLevel = this._visibleElements[i].indentLevel;
      if (nextLevel == thisLevel)
        return true;
      if (nextLevel < thisLevel)
        break;
    }
    return false;
  },

  getLevel: function PTV_getLevel(aRow) {
    this._ensureValidRow(aRow);

    // Level is 0 for nodes at the root level, 1 for its children and so on.
    return this._visibleElements[aRow].indentLevel;
  },

  getImageSrc: function PTV_getImageSrc(aRow, aColumn) {
    this._ensureValidRow(aRow);

    // only the title column has an image
    if (this._getColumnType(aColumn) != this.COLUMN_TYPE_TITLE)
      return "";

    return this._visibleElements[aRow].icon;
  },

  getProgressMode: function(aRow, aColumn) { },
  getCellValue: function(aRow, aColumn) { },

  getCellText: function PTV_getCellText(aRow, aColumn) {
    this._ensureValidRow(aRow);

    var node = this._visibleElements[aRow];
    var columnType = this._getColumnType(aColumn);
    switch (columnType) {
      case this.COLUMN_TYPE_TITLE:
        // normally, this is just the title, but we don't want empty items in
        // the tree view so return a special string if the title is empty.
        // Do it here so that callers can still get at the 0 length title
        // if they go through the "result" API.
        if (PlacesUtils.nodeIsSeparator(node))
          return "";
        return PlacesUIUtils.getBestTitle(node);
      case this.COLUMN_TYPE_TAGS:
        return node.tags;
      case this.COLUMN_TYPE_URI:
        if (PlacesUtils.nodeIsURI(node))
          return node.uri;
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
      case this.COLUMN_TYPE_KEYWORD:
        if (PlacesUtils.nodeIsBookmark(node))
          return PlacesUtils.bookmarks.getKeywordForBookmark(node.itemId);
        return "";
      case this.COLUMN_TYPE_DESCRIPTION:
        if (node.itemId != -1) {
          try {
            return PlacesUtils.annotations.
                               getItemAnnotation(node.itemId, DESCRIPTION_ANNO);
          }
          catch (ex) { /* has no description */ }
        }
        return "";
      case this.COLUMN_TYPE_DATEADDED:
        if (node.dateAdded)
          return this._convertPRTimeToString(node.dateAdded);
        return "";
      case this.COLUMN_TYPE_LASTMODIFIED:
        if (node.lastModified)
          return this._convertPRTimeToString(node.lastModified);
        return "";
    }
    return "";
  },

  setTree: function PTV_setTree(aTree) {
    var hasOldTree = this._tree != null;
    this._tree = aTree;

    if (this._result) {
      if (hasOldTree) {
        // detach from result when we are detaching from the tree.
        // This breaks the reference cycle between us and the result.
        if (!aTree)
          this._result.viewer = null;
      }
      if (aTree)
        this._finishInit();
    }
  },

  toggleOpenState: function PTV_toggleOpenState(aRow) {
    if (!this._result)
      throw Cr.NS_ERROR_UNEXPECTED;
    this._ensureValidRow(aRow);

    var node = this._visibleElements[aRow];
    if (this._flatList && this._openContainerCallback) {
      this._openContainerCallback(node);
      return;
    }

    var resource = this._getResourceForNode(node);
    if (resource) {
      const openLiteral = PlacesUIUtils.RDF.GetResource("http://home.netscape.com/NC-rdf#open");
      const trueLiteral = PlacesUIUtils.RDF.GetLiteral("true");

      if (node.containerOpen)
        PlacesUIUtils.localStore.Unassert(resource, openLiteral, trueLiteral);
      else
        PlacesUIUtils.localStore.Assert(resource, openLiteral, trueLiteral, true);
    }

    node.containerOpen = !node.containerOpen;
  },

  cycleHeader: function PTV_cycleHeader(aColumn) {
    if (!this._result)
      throw Cr.NS_ERROR_UNEXPECTED;

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
    var oldSortingAnnotation = this._result.sortingAnnotation;
    var newSort;
    var newSortingAnnotation = "";
    const NHQO = Ci.nsINavHistoryQueryOptions;
    var columnType = this._getColumnType(aColumn);
    switch (columnType) {
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
      case this.COLUMN_TYPE_KEYWORD:
        if (oldSort == NHQO.SORT_BY_KEYWORD_ASCENDING)
          newSort = NHQO.SORT_BY_KEYWORD_DESCENDING;
        else if (allowTriState && oldSort == NHQO.SORT_BY_KEYWORD_DESCENDING)
          newSort = NHQO.SORT_BY_NONE;
        else
          newSort = NHQO.SORT_BY_KEYWORD_ASCENDING;

        break;
      case this.COLUMN_TYPE_DESCRIPTION:
        if (oldSort == NHQO.SORT_BY_ANNOTATION_ASCENDING &&
            oldSortingAnnotation == DESCRIPTION_ANNO) {
          newSort = NHQO.SORT_BY_ANNOTATION_DESCENDING;
          newSortingAnnotation = DESCRIPTION_ANNO;
        }
        else if (allowTriState &&
                 oldSort == NHQO.SORT_BY_ANNOTATION_DESCENDING &&
                 oldSortingAnnotation == DESCRIPTION_ANNO)
          newSort = NHQO.SORT_BY_NONE;
        else {
          newSort = NHQO.SORT_BY_ANNOTATION_ASCENDING;
          newSortingAnnotation = DESCRIPTION_ANNO;
        }

        break;
      case this.COLUMN_TYPE_DATEADDED:
        if (oldSort == NHQO.SORT_BY_DATEADDED_ASCENDING)
          newSort = NHQO.SORT_BY_DATEADDED_DESCENDING;
        else if (allowTriState &&
                 oldSort == NHQO.SORT_BY_DATEADDED_DESCENDING)
          newSort = NHQO.SORT_BY_NONE;
        else
          newSort = NHQO.SORT_BY_DATEADDED_ASCENDING;

        break;
      case this.COLUMN_TYPE_LASTMODIFIED:
        if (oldSort == NHQO.SORT_BY_LASTMODIFIED_ASCENDING)
          newSort = NHQO.SORT_BY_LASTMODIFIED_DESCENDING;
        else if (allowTriState &&
                 oldSort == NHQO.SORT_BY_LASTMODIFIED_DESCENDING)
          newSort = NHQO.SORT_BY_NONE;
        else
          newSort = NHQO.SORT_BY_LASTMODIFIED_ASCENDING;

        break;
      case this.COLUMN_TYPE_TAGS:
        if (oldSort == NHQO.SORT_BY_TAGS_ASCENDING)
          newSort = NHQO.SORT_BY_TAGS_DESCENDING;
        else if (allowTriState && oldSort == NHQO.SORT_BY_TAGS_DESCENDING)
          newSort = NHQO.SORT_BY_NONE;
        else
          newSort = NHQO.SORT_BY_TAGS_ASCENDING;

        break;
      default:
        throw Cr.NS_ERROR_INVALID_ARG;
    }
    this._result.sortingAnnotation = newSortingAnnotation;
    this._result.sortingMode = newSort;
  },

  isEditable: function PTV_isEditable(aRow, aColumn) {
    // At this point we only support editing the title field.
    if (aColumn.index != 0)
      return false;

    var node = this.nodeForTreeIndex(aRow);
    if (!PlacesUtils.nodeIsReadOnly(node) &&
        (PlacesUtils.nodeIsFolder(node) ||
         (PlacesUtils.nodeIsBookmark(node) &&
          !PlacesUtils.nodeIsLivemarkItem(node))))
      return true;

    return false;
  },

  setCellText: function PTV_setCellText(aRow, aColumn, aText) {
    // we may only get here if the cell is editable
    var node = this.nodeForTreeIndex(aRow);
    if (node.title != aText) {
      var txn = PlacesUIUtils.ptm.editItemTitle(node.itemId, aText);
      PlacesUIUtils.ptm.doTransaction(txn);
    }
  },

  selectionChanged: function() { },
  cycleCell: function PTV_cycleCell(aRow, aColumn) { },
  isSelectable: function(aRow, aColumn) { return false; },
  performAction: function(aAction) { },
  performActionOnRow: function(aAction, aRow) { },
  performActionOnCell: function(aAction, aRow, aColumn) { }
};

function PlacesTreeView(aFlatList, aOnOpenFlatContainer) {
  this._tree = null;
  this._result = null;
  this._selection = null;
  this._visibleElements = [];
  this._flatList = aFlatList;
  this._openContainerCallback = aOnOpenFlatContainer;
}
