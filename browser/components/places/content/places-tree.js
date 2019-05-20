/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from controller.js */
/* import-globals-from treeView.js */

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  class MozPlacesTree extends customElements.get("tree") {
    constructor() {
      super();

      this.addEventListener("focus", (event) => {
        this._cachedInsertionPoint = undefined;

        // See select handler. We need the sidebar's places commandset to be
        // updated as well
        document.commandDispatcher.updateCommands("focus");
      });

      this.addEventListener("select", (event) => {
        this._cachedInsertionPoint = undefined;

        // This additional complexity is here for the sidebars
        var win = window;
        while (true) {
          win.document.commandDispatcher.updateCommands("focus");
          if (win == window.top)
            break;

          win = win.parent;
        }
      });

      this.addEventListener("dragstart", (event) => {
        if (event.target.localName != "treechildren")
          return;

        if (this.disableUserActions) {
          event.preventDefault();
          event.stopPropagation();
          return;
        }

        let nodes = this.selectedNodes;
        for (let i = 0; i < nodes.length; i++) {
          let node = nodes[i];

          // Disallow dragging the root node of a tree.
          if (!node.parent) {
            event.preventDefault();
            event.stopPropagation();
            return;
          }

          // If this node is child of a readonly container or cannot be moved,
          // we must force a copy.
          if (!this.controller.canMoveNode(node)) {
            event.dataTransfer.effectAllowed = "copyLink";
            break;
          }
        }

        this._controller.setDataTransfer(event);
        event.stopPropagation();
      });

      this.addEventListener("dragover", (event) => {
        if (event.target.localName != "treechildren")
          return;

        let cell = this.getCellAt(event.clientX, event.clientY);
        let node = cell.row != -1 ?
          this.view.nodeForTreeIndex(cell.row) :
          this.result.root;
        // cache the dropTarget for the view
        PlacesControllerDragHelper.currentDropTarget = node;

        // We have to calculate the orientation since view.canDrop will use
        // it and we want to be consistent with the dropfeedback.
        let rowHeight = this.rowHeight;
        let eventY = event.clientY - this.treeBody.getBoundingClientRect().y -
          rowHeight * (cell.row - this.getFirstVisibleRow());

        let orientation = Ci.nsITreeView.DROP_BEFORE;

        if (cell.row == -1) {
          // If the row is not valid we try to insert inside the resultNode.
          orientation = Ci.nsITreeView.DROP_ON;
        } else if (PlacesUtils.nodeIsContainer(node) &&
          eventY > rowHeight * 0.75) {
          // If we are below the 75% of a container the treeview we try
          // to drop after the node.
          orientation = Ci.nsITreeView.DROP_AFTER;
        } else if (PlacesUtils.nodeIsContainer(node) &&
          eventY > rowHeight * 0.25) {
          // If we are below the 25% of a container the treeview we try
          // to drop inside the node.
          orientation = Ci.nsITreeView.DROP_ON;
        }

        if (!this.view.canDrop(cell.row, orientation, event.dataTransfer))
          return;

        event.preventDefault();
        event.stopPropagation();
      });

      this.addEventListener("dragend", (event) => {
        PlacesControllerDragHelper.currentDropTarget = null;
      });
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }
      super.connectedCallback();
      this._contextMenuShown = false;

      this._active = true;

      // Force an initial build.
      if (this.place)
        this.place = this.place;
    }

    get controller() {
      return this._controller;
    }

    set disableUserActions(val) {
      if (val) this.setAttribute("disableUserActions", "true");
      else this.removeAttribute("disableUserActions");
      return val;
    }

    get disableUserActions() {
      return this.getAttribute("disableUserActions") == "true";
    }
    /**
     * overriding
     */
    set view(val) {
      /* eslint-disable no-undef */
      return Object.getOwnPropertyDescriptor(XULTreeElement.prototype, "view").set.call(this, val);
      /* eslint-enable no-undef */
    }

    get view() {
      try {
        /* eslint-disable no-undef */
        return Object.getOwnPropertyDescriptor(XULTreeElement.prototype, "view").get.
        call(this).wrappedJSObject || null;
        /* eslint-enable no-undef */
      } catch (e) {
        return null;
      }
    }

    get associatedElement() {
      return this;
    }

    set flatList(val) {
      if (this.flatList != val) {
        this.setAttribute("flatList", val);
        // reload with the last place set
        if (this.place)
          this.place = this.place;
      }
      return val;
    }

    get flatList() {
      return this.getAttribute("flatList") == "true";
    }

    /**
     * nsIPlacesView
     */
    get result() {
      try {
        return this.view.QueryInterface(Ci.nsINavHistoryResultObserver).result;
      } catch (e) {
        return null;
      }
    }
    /**
     * nsIPlacesView
     */
    set place(val) {
      this.setAttribute("place", val);

      let query = {},
        options = {};
      PlacesUtils.history.queryStringToQuery(val, query, options);
      this.load(query.value, options.value);

      return val;
    }

    get place() {
      return this.getAttribute("place");
    }
    /**
     * nsIPlacesView
     */
    get hasSelection() {
      return this.view && this.view.selection.count >= 1;
    }
    /**
     * nsIPlacesView
     */
    get selectedNodes() {
      let nodes = [];
      if (!this.hasSelection)
        return nodes;

      let selection = this.view.selection;
      let rc = selection.getRangeCount();
      let resultview = this.view;
      for (let i = 0; i < rc; ++i) {
        let min = {},
          max = {};
        selection.getRangeAt(i, min, max);
        for (let j = min.value; j <= max.value; ++j) {
          nodes.push(resultview.nodeForTreeIndex(j));
        }
      }
      return nodes;
    }
    /**
     * nsIPlacesView
     */
    get removableSelectionRanges() {
      // This property exists in addition to selectedNodes because it
      // encodes selection ranges (which only occur in list views) into
      // the return value. For each removed range, the index at which items
      // will be re-inserted upon the remove transaction being performed is
      // the first index of the range, so that the view updates correctly.
      //
      // For example, if we remove rows 2,3,4 and 7,8 from a list, when we
      // undo that operation, if we insert what was at row 3 at row 3 again,
      // it will show up _after_ the item that was at row 5. So we need to
      // insert all items at row 2, and the tree view will update correctly.
      //
      // Also, this function collapses the selection to remove redundant
      // data, e.g. when deleting this selection:
      //
      //      http://www.foo.com/
      //  (-) Some Folder
      //        http://www.bar.com/
      //
      // ... returning http://www.bar.com/ as part of the selection is
      // redundant because it is implied by removing "Some Folder". We
      // filter out all such redundancies since some partial amount of
      // the folder's children may be selected.
      //
      let nodes = [];
      if (!this.hasSelection)
        return nodes;

      var selection = this.view.selection;
      var rc = selection.getRangeCount();
      var resultview = this.view;
      // This list is kept independently of the range selected (i.e. OUTSIDE
      // the for loop) since the row index of a container is unique for the
      // entire view, and we could have some really wacky selection and we
      // don't want to blow up.
      var containers = {};
      for (var i = 0; i < rc; ++i) {
        var range = [];
        var min = {},
          max = {};
        selection.getRangeAt(i, min, max);

        for (var j = min.value; j <= max.value; ++j) {
          if (this.view.isContainer(j))
            containers[j] = true;
          if (!(this.view.getParentIndex(j) in containers))
            range.push(resultview.nodeForTreeIndex(j));
        }
        nodes.push(range);
      }
      return nodes;
    }
    /**
     * nsIPlacesView
     */
    get draggableSelection() {
      return this.selectedNodes;
    }
    /**
     * nsIPlacesView
     */
    get selectedNode() {
      var view = this.view;
      if (!view || view.selection.count != 1)
        return null;

      var selection = view.selection;
      var min = {},
        max = {};
      selection.getRangeAt(0, min, max);

      return this.view.nodeForTreeIndex(min.value);
    }
    /**
     * nsIPlacesView
     */
    get insertionPoint() {
      // invalidated on selection and focus changes
      if (this._cachedInsertionPoint !== undefined)
        return this._cachedInsertionPoint;

      // there is no insertion point for history queries
      // so bail out now and save a lot of work when updating commands
      var resultNode = this.result.root;
      if (PlacesUtils.nodeIsQuery(resultNode) &&
        PlacesUtils.asQuery(resultNode).queryOptions.queryType ==
        Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY)
        return this._cachedInsertionPoint = null;

      var orientation = Ci.nsITreeView.DROP_BEFORE;
      // If there is no selection, insert at the end of the container.
      if (!this.hasSelection) {
        var index = this.view.rowCount - 1;
        this._cachedInsertionPoint =
          this._getInsertionPoint(index, orientation);
        return this._cachedInsertionPoint;
      }

      // This is a two-part process. The first part is determining the drop
      // orientation.
      // * The default orientation is to drop _before_ the selected item.
      // * If the selected item is a container, the default orientation
      //   is to drop _into_ that container.
      //
      // Warning: It may be tempting to use tree indexes in this code, but
      //          you must not, since the tree is nested and as your tree
      //          index may change when folders before you are opened and
      //          closed. You must convert your tree index to a node, and
      //          then use getChildIndex to find your absolute index in
      //          the parent container instead.
      //
      var resultView = this.view;
      var selection = resultView.selection;
      var rc = selection.getRangeCount();
      var min = {},
        max = {};
      selection.getRangeAt(rc - 1, min, max);

      // If the sole selection is a container, and we are not in
      // a flatlist, insert into it.
      // Note that this only applies to _single_ selections,
      // if the last element within a multi-selection is a
      // container, insert _adjacent_ to the selection.
      //
      // If the sole selection is the bookmarks toolbar folder, we insert
      // into it even if it is not opened
      if (selection.count == 1 && resultView.isContainer(max.value) &&
        !this.flatList)
        orientation = Ci.nsITreeView.DROP_ON;

      this._cachedInsertionPoint =
        this._getInsertionPoint(max.value, orientation);
      return this._cachedInsertionPoint;
    }

    get ownerWindow() {
      return window;
    }

    set active(val) {
      return this._active = val;
    }

    get active() {
      return this._active;
    }

    applyFilter(filterString, folderRestrict, includeHidden) {
      // preserve grouping
      var queryNode = PlacesUtils.asQuery(this.result.root);
      var options = queryNode.queryOptions.clone();

      // Make sure we're getting uri results.
      // We do not yet support searching into grouped queries or into
      // tag containers, so we must fall to the default case.
      if (PlacesUtils.nodeIsHistoryContainer(queryNode) ||
        PlacesUtils.nodeIsTagQuery(queryNode) ||
        options.resultType == options.RESULTS_AS_TAGS_ROOT ||
        options.resultType == options.RESULTS_AS_ROOTS_QUERY)
        options.resultType = options.RESULTS_AS_URI;

      var query = PlacesUtils.history.getNewQuery();
      query.searchTerms = filterString;

      if (folderRestrict) {
        query.setParents(folderRestrict);
        options.queryType = options.QUERY_TYPE_BOOKMARKS;
      }

      options.includeHidden = !!includeHidden;

      this.load(query, options);
    }

    load(query, options) {
      let result = PlacesUtils.history
        .executeQuery(query, options);

      if (!this._controller) {
        this._controller = new PlacesController(this);
        this._controller.disableUserActions = this.disableUserActions;
        this.controllers.appendController(this._controller);
      }

      let treeView = new PlacesTreeView(this);

      // Observer removal is done within the view itself.  When the tree
      // goes away, view.setTree(null) is called, which then
      // calls removeObserver.
      result.addObserver(treeView);
      this.view = treeView;

      if (this.getAttribute("selectfirstnode") == "true" && treeView.rowCount > 0) {
        treeView.selection.select(0);
      }

      this._cachedInsertionPoint = undefined;
    }

    /**
     * Causes a particular node represented by the specified placeURI to be
     * selected in the tree. All containers above the node in the hierarchy
     * will be opened, so that the node is visible.
     */
    selectPlaceURI(placeURI) {
      // Do nothing if a node matching the given uri is already selected
      if (this.hasSelection && this.selectedNode.uri == placeURI)
        return;

      function findNode(container, nodesURIChecked) {
        var containerURI = container.uri;
        if (containerURI == placeURI)
          return container;
        if (nodesURIChecked.includes(containerURI))
          return null;

        // never check the contents of the same query
        nodesURIChecked.push(containerURI);

        var wasOpen = container.containerOpen;
        if (!wasOpen)
          container.containerOpen = true;
        for (var i = 0; i < container.childCount; ++i) {
          var child = container.getChild(i);
          var childURI = child.uri;
          if (childURI == placeURI) {
            return child;
          } else if (PlacesUtils.nodeIsContainer(child)) {
            var nested = findNode(PlacesUtils.asContainer(child), nodesURIChecked);
            if (nested)
              return nested;
          }
        }

        if (!wasOpen)
          container.containerOpen = false;

        return null;
      }

      var container = this.result.root;
      console.assert(container, "No result, cannot select place URI!");
      if (!container)
        return;

      var child = findNode(container, []);
      if (child) {
        this.selectNode(child);
      } else {
        // If the specified child could not be located, clear the selection
        var selection = this.view.selection;
        selection.clearSelection();
      }
    }

    /**
     * Causes a particular node to be selected in the tree, resulting in all
     * containers above the node in the hierarchy to be opened, so that the
     * node is visible.
     */
    selectNode(node) {
      var view = this.view;

      var parent = node.parent;
      if (parent && !parent.containerOpen) {
        // Build a list of all of the nodes that are the parent of this one
        // in the result.
        var parents = [];
        var root = this.result.root;
        while (parent && parent != root) {
          parents.push(parent);
          parent = parent.parent;
        }

        // Walk the list backwards (opening from the root of the hierarchy)
        // opening each folder as we go.
        for (var i = parents.length - 1; i >= 0; --i) {
          let index = view.treeIndexForNode(parents[i]);
          if (index != -1 &&
            view.isContainer(index) && !view.isContainerOpen(index))
            view.toggleOpenState(index);
        }
        // Select the specified node...
      }

      let index = view.treeIndexForNode(node);
      if (index == -1)
        return;

      view.selection.select(index);
      // ... and ensure it's visible, not scrolled off somewhere.
      this.ensureRowIsVisible(index);
    }

    toggleCutNode(aNode, aValue) {
      this.view.toggleCutNode(aNode, aValue);
    }

    _getInsertionPoint(index, orientation) {
      var result = this.result;
      var resultview = this.view;
      var container = result.root;
      var dropNearNode = null;
      console.assert(container, "null container");
      // When there's no selection, assume the container is the container
      // the view is populated from (i.e. the result's itemId).
      if (index != -1) {
        var lastSelected = resultview.nodeForTreeIndex(index);
        if (resultview.isContainer(index) && orientation == Ci.nsITreeView.DROP_ON) {
          // If the last selected item is an open container, append _into_
          // it, rather than insert adjacent to it.
          container = lastSelected;
          index = -1;
        } else if (lastSelected.containerOpen &&
          orientation == Ci.nsITreeView.DROP_AFTER &&
          lastSelected.hasChildren) {
          // If the last selected item is an open container and the user is
          // trying to drag into it as a first item, really insert into it.
          container = lastSelected;
          orientation = Ci.nsITreeView.DROP_ON;
          index = 0;
        } else {
          // Use the last-selected node's container.
          container = lastSelected.parent;

          // See comment in the treeView.js's copy of this method
          if (!container || !container.containerOpen)
            return null;

          // Avoid the potentially expensive call to getChildIndex
          // if we know this container doesn't allow insertion
          if (this.controller.disallowInsertion(container))
            return null;

          var queryOptions = PlacesUtils.asQuery(result.root).queryOptions;
          if (queryOptions.sortingMode !=
            Ci.nsINavHistoryQueryOptions.SORT_BY_NONE) {
            // If we are within a sorted view, insert at the end
            index = -1;
          } else if (queryOptions.excludeItems ||
            queryOptions.excludeQueries) {
            // Some item may be invisible, insert near last selected one.
            // We don't replace index here to avoid requests to the db,
            // instead it will be calculated later by the controller.
            index = -1;
            dropNearNode = lastSelected;
          } else {
            var lsi = container.getChildIndex(lastSelected);
            index = orientation == Ci.nsITreeView.DROP_BEFORE ? lsi : lsi + 1;
          }
        }
      }

      if (this.controller.disallowInsertion(container))
        return null;

      let tagName = PlacesUtils.nodeIsTagQuery(container) ?
        PlacesUtils.asQuery(container).query.tags[0] : null;

      return new PlacesInsertionPoint({
        parentId: PlacesUtils.getConcreteItemId(container),
        parentGuid: PlacesUtils.getConcreteItemGuid(container),
        index,
        orientation,
        tagName,
        dropNearNode,
      });
    }

    /**
     * nsIPlacesView
     */
    selectAll() {
      this.view.selection.selectAll();
    }

    /**
     * This method will select the first node in the tree that matches
     * each given item guid. It will open any folder nodes that it needs
     * to in order to show the selected items.
     */
    selectItems(aGuids, aOpenContainers) {
      // Never open containers in flat lists.
      if (this.flatList)
        aOpenContainers = false;
      // By default, we do search and select within containers which were
      // closed (note that containers in which nodes were not found are
      // closed).
      if (aOpenContainers === undefined)
        aOpenContainers = true;

      var guids = aGuids; // don't manipulate the caller's array

      // Array of nodes found by findNodes which are to be selected
      var nodes = [];

      // Array of nodes found by findNodes which should be opened
      var nodesToOpen = [];

      // A set of GUIDs of container-nodes that were previously searched,
      // and thus shouldn't be searched again. This is empty at the initial
      // start of the recursion and gets filled in as the recursion
      // progresses.
      var checkedGuidsSet = new Set();

      /**
       * Recursively search through a node's children for items
       * with the given GUIDs. When a matching item is found, remove its GUID
       * from the GUIDs array, and add the found node to the nodes dictionary.
       *
       * NOTE: This method will leave open any node that had matching items
       * in its subtree.
       */
      function findNodes(node) {
        var foundOne = false;
        // See if node matches an ID we wanted; add to results.
        // For simple folder queries, check both itemId and the concrete
        // item id.
        var index = guids.indexOf(node.bookmarkGuid);
        if (index == -1) {
          let concreteGuid = PlacesUtils.getConcreteItemGuid(node);
          if (concreteGuid != node.bookmarkGuid) {
            index = guids.indexOf(concreteGuid);
          }
        }

        if (index != -1) {
          nodes.push(node);
          foundOne = true;
          guids.splice(index, 1);
        }

        var concreteGuid = PlacesUtils.getConcreteItemGuid(node);
        if (guids.length == 0 || !PlacesUtils.nodeIsContainer(node) ||
          checkedGuidsSet.has(concreteGuid))
          return foundOne;

        // Only follow a query if it has been been explicitly opened by the
        // caller. We support the "AllBookmarks" case to allow callers to
        // specify just the top-level bookmark folders.
        let shouldOpen = aOpenContainers && (PlacesUtils.nodeIsFolder(node) ||
          (PlacesUtils.nodeIsQuery(node) && node.bookmarkGuid == PlacesUIUtils.virtualAllBookmarksGuid));

        PlacesUtils.asContainer(node);
        if (!node.containerOpen && !shouldOpen)
          return foundOne;

        checkedGuidsSet.add(concreteGuid);

        // Remember the beginning state so that we can re-close
        // this node if we don't find any additional results here.
        var previousOpenness = node.containerOpen;
        node.containerOpen = true;
        for (var child = 0; child < node.childCount && guids.length > 0; child++) {
          var childNode = node.getChild(child);
          var found = findNodes(childNode);
          if (!foundOne)
            foundOne = found;
        }

        // If we didn't find any additional matches in this node's
        // subtree, revert the node to its previous openness.
        if (foundOne)
          nodesToOpen.unshift(node);
        node.containerOpen = previousOpenness;
        return foundOne;
      }

      // Disable notifications while looking for nodes.
      let result = this.result;
      let didSuppressNotifications = result.suppressNotifications;
      if (!didSuppressNotifications)
        result.suppressNotifications = true;
      try {
        findNodes(this.result.root);
      } finally {
        if (!didSuppressNotifications)
          result.suppressNotifications = false;
      }

      // For all the nodes we've found, highlight the corresponding
      // index in the tree.
      var resultview = this.view;
      var selection = this.view.selection;
      selection.selectEventsSuppressed = true;
      selection.clearSelection();
      // Open nodes containing found items
      for (let i = 0; i < nodesToOpen.length; i++) {
        nodesToOpen[i].containerOpen = true;
      }
      for (let i = 0; i < nodes.length; i++) {
        var index = resultview.treeIndexForNode(nodes[i]);
        if (index == -1)
          continue;
        selection.rangedSelect(index, index, true);
      }
      selection.selectEventsSuppressed = false;
    }

    buildContextMenu(aPopup) {
      this._contextMenuShown = true;
      return this.controller.buildContextMenu(aPopup);
    }

    destroyContextMenu(aPopup) {}
    disconnectedCallback() {
      // Unregister the controllber before unlinking the view, otherwise it
      // may still try to update commands on a view with a null result.
      if (this._controller) {
        this._controller.terminate();
        this.controllers.removeController(this._controller);
      }

      if (this.view) {
        this.view.uninit();
      }
      // view.setTree(null) will be called upon unsetting the view, which
      // breaks the reference cycle between the PlacesTreeView and result.
      // See the "setTree" method of PlacesTreeView in treeView.js.
      this.view = null;
    }
  }

  customElements.define("places-tree", MozPlacesTree, {
    extends: "tree",
  });
}
