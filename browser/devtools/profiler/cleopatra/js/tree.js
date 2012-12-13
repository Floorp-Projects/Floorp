/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var kMaxChunkDuration = 30; // ms

var escape = document.createElement('textarea');

function escapeHTML(html) {
  escape.innerHTML = html;
  return escape.innerHTML;
}

function unescapeHTML(html) {
  escape.innerHTML = html;
  return escape.value;
}

RegExp.escape = function(text) {
    return text.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&");
}

var requestAnimationFrame_timeout = null;
var requestAnimationFrame = window.webkitRequestAnimationFrame ||
                            window.mozRequestAnimationFrame ||
                            window.oRequestAnimationFrame ||
                            window.msRequestAnimationFrame ||
                            function(callback, element) {
                              window.setTimeout(callback, 1000 / 60);
                            };

var cancelAnimationFrame = window.webkitCancelAnimationFrame ||
                           window.mozCancelAnimationFrame ||
                           window.oCancelAnimationFrame ||
                           window.msCancelAnimationFrame ||
                           function(callback, element) {
                             if (requestAnimationFrame_timeout) {
                               window.clearTimeout(requestAnimationFrame_timeout);
                               requestAnimationFrame_timeout = null;
                             }
                           };

function TreeView() {
  this._eventListeners = {};
  this._pendingActions = [];
  this._pendingActionsProcessingCallback = null;

  this._container = document.createElement("div");
  this._container.className = "treeViewContainer";
  this._container.setAttribute("tabindex", "0"); // make it focusable

  this._header = document.createElement("ul");
  this._header.className = "treeHeader";
  this._container.appendChild(this._header);

  this._verticalScrollbox = document.createElement("div");
  this._verticalScrollbox.className = "treeViewVerticalScrollbox";
  this._container.appendChild(this._verticalScrollbox);

  this._leftColumnBackground = document.createElement("div");
  this._leftColumnBackground.className = "leftColumnBackground";
  this._verticalScrollbox.appendChild(this._leftColumnBackground);

  this._horizontalScrollbox = document.createElement("div");
  this._horizontalScrollbox.className = "treeViewHorizontalScrollbox";
  this._verticalScrollbox.appendChild(this._horizontalScrollbox);

  this._contextMenu = document.createElement("menu");
  this._contextMenu.setAttribute("type", "context");
  this._contextMenu.id = "contextMenuForTreeView" + TreeView.instanceCounter++;
  this._container.appendChild(this._contextMenu);

  this._busyCover = document.createElement("div");
  this._busyCover.className = "busyCover";
  this._container.appendChild(this._busyCover);
  this._abortToggleAll = false;

  var self = this;
  this._container.onkeydown = function (e) {
    self._onkeypress(e);
  };
  this._container.onclick = function (e) {
    self._onclick(e);
  };
  this._verticalScrollbox.addEventListener("contextmenu", function(event) {
    self._populateContextMenu(event);
  }, true);
  this._setUpScrolling();
};
TreeView.instanceCounter = 0;

TreeView.prototype = {
  getContainer: function TreeView_getContainer() {
    return this._container;
  },
  setColumns: function TreeView_setColumns(columns) {
    this._header.innerHTML = "";
    for (var i = 0; i < columns.length; i++) {
      var li = document.createElement("li");
      li.className = "treeColumnHeader treeColumnHeader" + i;
      li.id = columns[i].name + "Header";
      li.textContent = columns[i].title;
      this._header.appendChild(li);
    }
  },
  dataIsOutdated: function TreeView_dataIsOutdated() {
    this._busyCover.classList.add("busy");
  },
  display: function TreeView_display(data, filterByName) {
    this._busyCover.classList.remove("busy");
    this._filterByName = filterByName;
    this._filterByNameReg = null; // lazy init
    if (this._filterByName === "")
      this._filterByName = null;
    this._horizontalScrollbox.innerHTML = "";
    this._horizontalScrollbox.data = data[0].getData();
    if (this._pendingActionsProcessingCallback) {
      cancelAnimationFrame(this._pendingActionsProcessingCallback);
      this._pendingActionsProcessingCallback = 0;
    }
    this._pendingActions = [];

    this._pendingActions.push({
      parentElement: this._horizontalScrollbox,
      parentNode: null,
      data: data[0].getData()
    });
    this._processPendingActionsChunk();
    this._select(this._horizontalScrollbox.firstChild);
    this._toggle(this._horizontalScrollbox.firstChild);
    this._container.focus();
  },
  // Provide a snapshot of the reverse selection to restore with 'invert callback'
  getReverseSelectionSnapshot: function TreeView__getReverseSelectionSnapshot(isJavascriptOnly) {
    if (!this._selectedNode)
      return;
    var snapshot = [];
    var curr = this._selectedNode.data;

    while(curr) {
      if (isJavascriptOnly && curr.isJSFrame || !isJavascriptOnly) {
        snapshot.push(curr.name);
        //dump(JSON.stringify(curr.name) + "\n");
      }
      if (curr.children && curr.children.length >= 1) {
        curr = curr.children[0].getData();
      } else {
        break;
      }
    }

    return snapshot.reverse();
  },
  // Provide a snapshot of the current selection to restore
  getSelectionSnapshot: function TreeView__getSelectionSnapshot(isJavascriptOnly) {
    var snapshot = [];
    var curr = this._selectedNode;

    while(curr) {
      if (isJavascriptOnly && curr.data.isJSFrame || !isJavascriptOnly) {
        snapshot.push(curr.data.name);
        //dump(JSON.stringify(curr.data.name) + "\n");
      }
      curr = curr.treeParent;
    }

    return snapshot.reverse();
  },
  setSelection: function TreeView_setSelection(frames) {
    this.restoreSelectionSnapshot(frames, false);
  },
  // Take a selection snapshot and restore the selection
  restoreSelectionSnapshot: function TreeView_restoreSelectionSnapshot(snapshot, allowNonContigious) {
    var currNode = this._horizontalScrollbox.firstChild;
    if (currNode.data.name == snapshot[0] || snapshot[0] == "(total)") {
      snapshot.shift();
    }
    //dump("len: " + snapshot.length + "\n");
    next_level: while (currNode && snapshot.length > 0) {
      this._toggle(currNode, false, true);
      this._syncProcessPendingActionProcessing();
      for (var i = 0; i < currNode.treeChildren.length; i++) {
        if (currNode.treeChildren[i].data.name == snapshot[0]) {
          //dump("Found: " + currNode.treeChildren[i].data.name + "\n");
          snapshot.shift();
          this._toggle(currNode, false, true);
          currNode = currNode.treeChildren[i];
          continue next_level;
        }
      }
      if (allowNonContigious === true) {
        // We need to do a Breadth-first search to find a match
        var pendingSearch = [currNode.data];
        while (pendingSearch.length > 0) {
          var node = pendingSearch.shift();
          //dump("searching: " + node.name + " for: " + snapshot[0] + "\n");
          if (!node.children)
            continue;
          for (var i = 0; i < node.children.length; i++) {
            var childNode = node.children[i].getData();
            if (childNode.name == snapshot[0]) {
              //dump("found: " + childNode.name + "\n");
              snapshot.shift();
              var nodesToToggle = [childNode];
              while (nodesToToggle[0].name != currNode.data.name) {
                nodesToToggle.splice(0, 0, nodesToToggle[0].parent);
              }
              var lastToggle = currNode;
              for (var j = 0; j < nodesToToggle.length; j++) {
                for (var k = 0; k < lastToggle.treeChildren.length; k++) {
                  if (lastToggle.treeChildren[k].data.name == nodesToToggle[j].name) {
                    //dump("Expend: " + nodesToToggle[j].name + "\n");
                    this._toggle(lastToggle.treeChildren[k], false, true);
                    lastToggle = lastToggle.treeChildren[k];
                    this._syncProcessPendingActionProcessing();
                  }
                }
              }
              currNode = lastToggle;
              continue next_level;
            }
            //dump("pending: " + childNode.name + "\n");
            pendingSearch.push(childNode);
          }
        }
      }
      break; // Didn't find child node matching
    }

    if (currNode == this._horizontalScrollbox) {
      PROFILERERROR("Failed to restore selection, could not find root.\n");
      return;
    }

    this._toggle(currNode, true, true);
    this._select(currNode);
  },
  _processPendingActionsChunk: function TreeView__processPendingActionsChunk(isSync) {
    this._pendingActionsProcessingCallback = 0;

    var startTime = Date.now();
    var endTime = startTime + kMaxChunkDuration;
    while ((isSync == true || Date.now() < endTime) && this._pendingActions.length > 0) {
      this._processOneAction(this._pendingActions.shift());
    }
    this._scrollHeightChanged();

    this._schedulePendingActionProcessing();
  },
  _schedulePendingActionProcessing: function TreeView__schedulePendingActionProcessing() {
    if (!this._pendingActionsProcessingCallback && this._pendingActions.length > 0) {
      var self = this;
      this._pendingActionsProcessingCallback = requestAnimationFrame(function () {
        self._processPendingActionsChunk();
      });
    }
  },
  _syncProcessPendingActionProcessing: function TreeView__syncProcessPendingActionProcessing() {
    this._processPendingActionsChunk(true);
  },
  _processOneAction: function TreeView__processOneAction(action) {
    var li = this._createTree(action.parentElement, action.parentNode, action.data);
    if ("allChildrenCollapsedValue" in action) {
      if (this._abortToggleAll)
        return;
      this._toggleAll(li, action.allChildrenCollapsedValue, true);
    }
  },
  addEventListener: function TreeView_addEventListener(eventName, callbackFunction) {
    if (!(eventName in this._eventListeners))
      this._eventListeners[eventName] = [];
    if (this._eventListeners[eventName].indexOf(callbackFunction) != -1)
      return;
    this._eventListeners[eventName].push(callbackFunction);
  },
  removeEventListener: function TreeView_removeEventListener(eventName, callbackFunction) {
    if (!(eventName in this._eventListeners))
      return;
    var index = this._eventListeners[eventName].indexOf(callbackFunction);
    if (index == -1)
      return;
    this._eventListeners[eventName].splice(index, 1);
  },
  _fireEvent: function TreeView__fireEvent(eventName, eventObject) {
    if (!(eventName in this._eventListeners))
      return;
    this._eventListeners[eventName].forEach(function (callbackFunction) {
      callbackFunction(eventObject);
    });
  },
  _setUpScrolling: function TreeView__setUpScrolling() {
    var waitingForPaint = false;
    var accumulatedDeltaX = 0;
    var accumulatedDeltaY = 0;
    var self = this;
    function scrollListener(e) {
      if (!waitingForPaint) {
        requestAnimationFrame(function () {
          self._horizontalScrollbox.scrollLeft += accumulatedDeltaX;
          self._verticalScrollbox.scrollTop += accumulatedDeltaY;
          accumulatedDeltaX = 0;
          accumulatedDeltaY = 0;
          waitingForPaint = false;
        });
        waitingForPaint = true;
      }
      if (e.axis == e.HORIZONTAL_AXIS) {
        accumulatedDeltaX += e.detail;
      } else {
        accumulatedDeltaY += e.detail;
      }
      e.preventDefault();
    }
    this._verticalScrollbox.addEventListener("MozMousePixelScroll", scrollListener, false);
    this._verticalScrollbox.cleanUp = function () {
      self._verticalScrollbox.removeEventListener("MozMousePixelScroll", scrollListener, false);
    };
  },
  _scrollHeightChanged: function TreeView__scrollHeightChanged() {
    this._leftColumnBackground.style.height = this._horizontalScrollbox.getBoundingClientRect().height + 'px';
  },
  _createTree: function TreeView__createTree(parentElement, parentNode, data) {
    var div = document.createElement("div");
    div.className = "treeViewNode collapsed";
    var hasChildren = ("children" in data) && (data.children.length > 0);
    if (!hasChildren)
      div.classList.add("leaf");
    var treeLine = document.createElement("div");
    treeLine.className = "treeLine";
    treeLine.innerHTML = this._HTMLForFunction(data);
    // When this item is toggled we will expand its children
    div.pendingExpand = [];
    div.treeLine = treeLine;
    div.data = data;
    div.appendChild(treeLine);
    div.treeChildren = [];
    div.treeParent = parentNode;
    if (hasChildren) {
      var parent = document.createElement("div");
      parent.className = "treeViewNodeList";
      for (var i = 0; i < data.children.length; ++i) {
        div.pendingExpand.push({parentElement: parent, parentNode: div, data: data.children[i].getData() });
      }
      div.appendChild(parent);
    }
    if (parentNode) {
      parentNode.treeChildren.push(div);
    }
    parentElement.appendChild(div);
    return div;
  },
  _populateContextMenu: function TreeView__populateContextMenu(event) {
    this._verticalScrollbox.setAttribute("contextmenu", "");

    var target = event.target;
    if (target.classList.contains("expandCollapseButton") ||
        target.classList.contains("focusCallstackButton"))
      return;

    var li = this._getParentTreeViewNode(target);
    if (!li)
      return;

    this._select(li);

    this._contextMenu.innerHTML = "";

    var self = this;
    this._contextMenuForFunction(li.data).forEach(function (menuItem) {
      var menuItemNode = document.createElement("menuitem");
      menuItemNode.onclick = (function (menuItem) {
        return function() {
          self._contextMenuClick(li.data, menuItem);
        };
      })(menuItem);
      menuItemNode.label = menuItem;
      self._contextMenu.appendChild(menuItemNode);
    });

    this._verticalScrollbox.setAttribute("contextmenu", this._contextMenu.id);
  },
  _contextMenuClick: function TreeView__contextMenuClick(node, menuItem) {
    this._fireEvent("contextMenuClick", { node: node, menuItem: menuItem });
  },
  _contextMenuForFunction: function TreeView__contextMenuForFunction(node) {
    // TODO move me outside tree.js
    var menu = [];
    if (node.library != null && (
      node.library.toLowerCase() == "xul" ||
      node.library.toLowerCase() == "xul.dll"
      )) {
      menu.push("View Source");
    }
    if (node.isJSFrame && node.scriptLocation) {
      menu.push("View JS Source");
    }
    menu.push("Focus Frame");
    menu.push("Focus Callstack");
    menu.push("Google Search");
    menu.push("Plugin View: Pie");
    menu.push("Plugin View: Tree");
    return menu;
  },
  _HTMLForFunction: function TreeView__HTMLForFunction(node) {
    var nodeName = escapeHTML(node.name);
    var libName = node.library;
    if (this._filterByName) {
      if (!this._filterByNameReg) {
        this._filterByName = RegExp.escape(this._filterByName);
        this._filterByNameReg = new RegExp("(" + this._filterByName + ")","gi");
      }
      nodeName = nodeName.replace(this._filterByNameReg, "<a style='color:red;'>$1</a>");
      libName = libName.replace(this._filterByNameReg, "<a style='color:red;'>$1</a>");
    }
    var samplePercentage;
    if (isNaN(node.ratio)) {
      samplePercentage = "";
    } else {
      samplePercentage = (100 * node.ratio).toFixed(1) + "%";
    }
    return '<input type="button" value="Expand / Collapse" class="expandCollapseButton" tabindex="-1"> ' +
      '<span class="sampleCount">' + node.counter + '</span> ' +
      '<span class="samplePercentage">' + samplePercentage + '</span> ' +
      '<span class="selfSampleCount">' + node.selfCounter + '</span> ' +
      '<span class="functionName">' + nodeName + '</span>' +
      '<span class="libraryName">' + libName + '</span>' +
      '<input type="button" value="Focus Callstack" title="Focus Callstack" class="focusCallstackButton" tabindex="-1">';
  },
  _resolveChildren: function TreeView__resolveChildren(div, childrenCollapsedValue) {
    while (div.pendingExpand != null && div.pendingExpand.length > 0) {
      var pendingExpand = div.pendingExpand.shift();
      pendingExpand.allChildrenCollapsedValue = childrenCollapsedValue;
      this._pendingActions.push(pendingExpand);
      this._schedulePendingActionProcessing();
    }
  },
  _toggle: function TreeView__toggle(div, /* optional */ newCollapsedValue, /* optional */ suppressScrollHeightNotification) {
    var currentCollapsedValue = this._isCollapsed(div);
    if (newCollapsedValue === undefined)
      newCollapsedValue = !currentCollapsedValue;
    if (newCollapsedValue) {
      div.classList.add("collapsed");
    } else {
      this._resolveChildren(div, true);
      div.classList.remove("collapsed");
    }
    if (!suppressScrollHeightNotification)
      this._scrollHeightChanged();
  },
  _toggleAll: function TreeView__toggleAll(subtreeRoot, /* optional */ newCollapsedValue, /* optional */ suppressScrollHeightNotification) {

    // Reset abort
    this._abortToggleAll = false;

    // Expands / collapses all child nodes, too.

    if (newCollapsedValue === undefined)
      newCollapsedValue = !this._isCollapsed(subtreeRoot);
    if (!newCollapsedValue) {
      // expanding
      this._resolveChildren(subtreeRoot, newCollapsedValue);
    }
    this._toggle(subtreeRoot, newCollapsedValue, true);
    for (var i = 0; i < subtreeRoot.treeChildren.length; ++i) {
      this._toggleAll(subtreeRoot.treeChildren[i], newCollapsedValue, true);
    }
    if (!suppressScrollHeightNotification)
      this._scrollHeightChanged();
  },
  _getParent: function TreeView__getParent(div) {
    return div.treeParent;
  },
  _getFirstChild: function TreeView__getFirstChild(div) {
    if (this._isCollapsed(div))
      return null;
    var child = div.treeChildren[0];
    return child;
  },
  _getLastChild: function TreeView__getLastChild(div) {
    if (this._isCollapsed(div))
      return div;
    var lastChild = div.treeChildren[div.treeChildren.length-1];
    if (lastChild == null)
      return div;
    return this._getLastChild(lastChild);
  },
  _getPrevSib: function TreeView__getPevSib(div) {
    if (div.treeParent == null)
      return null;
    var nodeIndex = div.treeParent.treeChildren.indexOf(div);
    if (nodeIndex == 0)
      return null;
    return div.treeParent.treeChildren[nodeIndex-1];
  },
  _getNextSib: function TreeView__getNextSib(div) {
    if (div.treeParent == null)
      return null;
    var nodeIndex = div.treeParent.treeChildren.indexOf(div);
    if (nodeIndex == div.treeParent.treeChildren.length - 1)
      return this._getNextSib(div.treeParent);
    return div.treeParent.treeChildren[nodeIndex+1];
  },
  _scrollIntoView: function TreeView__scrollIntoView(element, maxImportantWidth) {
    // Make sure that element is inside the visible part of our scrollbox by
    // adjusting the scroll positions. If element is wider or
    // higher than the scroll port, the left and top edges are prioritized over
    // the right and bottom edges.
    // If maxImportantWidth is set, parts of the beyond this widths are
    // considered as not important; they'll not be moved into view.

    if (maxImportantWidth === undefined)
      maxImportantWidth = Infinity;

    var visibleRect = {
      left: this._horizontalScrollbox.getBoundingClientRect().left + 150, // TODO: un-hardcode 150
      top: this._verticalScrollbox.getBoundingClientRect().top,
      right: this._horizontalScrollbox.getBoundingClientRect().right,
      bottom: this._verticalScrollbox.getBoundingClientRect().bottom
    }
    var r = element.getBoundingClientRect();
    var right = Math.min(r.right, r.left + maxImportantWidth);
    var leftCutoff = visibleRect.left - r.left;
    var rightCutoff = right - visibleRect.right;
    var topCutoff = visibleRect.top - r.top;
    var bottomCutoff = r.bottom - visibleRect.bottom;
    if (leftCutoff > 0)
      this._horizontalScrollbox.scrollLeft -= leftCutoff;
    else if (rightCutoff > 0)
      this._horizontalScrollbox.scrollLeft += Math.min(rightCutoff, -leftCutoff);
    if (topCutoff > 0)
      this._verticalScrollbox.scrollTop -= topCutoff;
    else if (bottomCutoff > 0)
      this._verticalScrollbox.scrollTop += Math.min(bottomCutoff, -topCutoff);
  },
  _select: function TreeView__select(li) {
    if (this._selectedNode != null) {
      this._selectedNode.treeLine.classList.remove("selected");
      this._selectedNode = null;
    }
    if (li) {
      li.treeLine.classList.add("selected");
      this._selectedNode = li;
      var functionName = li.treeLine.querySelector(".functionName");
      this._scrollIntoView(functionName, 400);
      this._fireEvent("select", li.data);
    }
  },
  _isCollapsed: function TreeView__isCollapsed(div) {
    return div.classList.contains("collapsed");
  },
  _getParentTreeViewNode: function TreeView__getParentTreeViewNode(node) {
    while (node) {
      if (node.nodeType != node.ELEMENT_NODE)
        break;
      if (node.classList.contains("treeViewNode"))
        return node;
      node = node.parentNode;
    }
    return null;
  },
  _onclick: function TreeView__onclick(event) {
    var target = event.target;
    var node = this._getParentTreeViewNode(target);
    if (!node)
      return;
    if (target.classList.contains("expandCollapseButton")) {
      if (event.altKey)
        this._toggleAll(node);
      else
        this._toggle(node);
    } else if (target.classList.contains("focusCallstackButton")) {
      this._fireEvent("focusCallstackButtonClicked", node.data);
    } else {
      this._select(node);
      if (event.detail == 2) // dblclick
        this._toggle(node);
    }
  },
  _onkeypress: function TreeView__onkeypress(event) {
    if (event.ctrlKey || event.altKey || event.metaKey)
      return;

    this._abortToggleAll = true;

    var selected = this._selectedNode;
    if (event.keyCode < 37 || event.keyCode > 40) {
      if (event.keyCode != 0 ||
          String.fromCharCode(event.charCode) != '*') {
        return;
      }
    }
    event.stopPropagation();
    event.preventDefault();
    if (!selected)
      return;
    if (event.keyCode == 37) { // KEY_LEFT
      var isCollapsed = this._isCollapsed(selected);
      if (!isCollapsed) {
        this._toggle(selected);
      } else {
        var parent = this._getParent(selected); 
        if (parent != null) {
          this._select(parent);
        }
      }
    } else if (event.keyCode == 38) { // KEY_UP
      var prevSib = this._getPrevSib(selected);
      var parent = this._getParent(selected); 
      if (prevSib != null) {
        this._select(this._getLastChild(prevSib));
      } else if (parent != null) {
        this._select(parent);
      }
    } else if (event.keyCode == 39) { // KEY_RIGHT
      var isCollapsed = this._isCollapsed(selected);
      if (isCollapsed) {
        this._toggle(selected);
      } else {
        // Do KEY_DOWN
        var nextSib = this._getNextSib(selected);
        var child = this._getFirstChild(selected); 
        if (child != null) {
          this._select(child);
        } else if (nextSib) {
          this._select(nextSib);
        }
      }
    } else if (event.keyCode == 40) { // KEY_DOWN
      var nextSib = this._getNextSib(selected);
      var child = this._getFirstChild(selected); 
      if (child != null) {
        this._select(child);
      } else if (nextSib) {
        this._select(nextSib);
      }
    } else if (String.fromCharCode(event.charCode) == '*') {
      this._toggleAll(selected);
    }
  },
};

