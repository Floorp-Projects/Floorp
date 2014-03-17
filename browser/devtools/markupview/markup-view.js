/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Cu, Ci} = require("chrome");

// Page size for pageup/pagedown
const PAGE_SIZE = 10;
const PREVIEW_AREA = 700;
const DEFAULT_MAX_CHILDREN = 100;
const COLLAPSE_ATTRIBUTE_LENGTH = 120;
const COLLAPSE_DATA_URL_REGEX = /^data.+base64/;
const COLLAPSE_DATA_URL_LENGTH = 60;
const CONTAINER_FLASHING_DURATION = 500;
const NEW_SELECTION_HIGHLIGHTER_TIMER = 1000;

const {UndoStack} = require("devtools/shared/undo");
const {editableField, InplaceEditor} = require("devtools/shared/inplace-editor");
const {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
const {HTMLEditor} = require("devtools/markupview/html-editor");
const promise = require("sdk/core/promise");
const {Tooltip} = require("devtools/shared/widgets/Tooltip");
const EventEmitter = require("devtools/toolkit/event-emitter");

Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm");
Cu.import("resource://gre/modules/devtools/Templater.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

loader.lazyGetter(this, "DOMParser", function() {
 return Cc["@mozilla.org/xmlextras/domparser;1"].createInstance(Ci.nsIDOMParser);
});
loader.lazyGetter(this, "AutocompletePopup", () => {
  return require("devtools/shared/autocomplete-popup").AutocompletePopup
});

/**
 * Vocabulary for the purposes of this file:
 *
 * MarkupContainer - the structure that holds an editor and its
 *  immediate children in the markup panel.
 * Node - A content node.
 * object.elt - A UI element in the markup panel.
 */

/**
 * The markup tree.  Manages the mapping of nodes to MarkupContainers,
 * updating based on mutations, and the undo/redo bindings.
 *
 * @param Inspector aInspector
 *        The inspector we're watching.
 * @param iframe aFrame
 *        An iframe in which the caller has kindly loaded markup-view.xhtml.
 */
function MarkupView(aInspector, aFrame, aControllerWindow) {
  this._inspector = aInspector;
  this.walker = this._inspector.walker;
  this._frame = aFrame;
  this.doc = this._frame.contentDocument;
  this._elt = this.doc.querySelector("#root");
  this.htmlEditor = new HTMLEditor(this.doc);

  this.layoutHelpers = new LayoutHelpers(this.doc.defaultView);

  try {
    this.maxChildren = Services.prefs.getIntPref("devtools.markup.pagesize");
  } catch(ex) {
    this.maxChildren = DEFAULT_MAX_CHILDREN;
  }

  // Creating the popup to be used to show CSS suggestions.
  let options = {
    autoSelect: true,
    theme: "auto"
  };
  this.popup = new AutocompletePopup(this.doc.defaultView.parent.document, options);

  this.undo = new UndoStack();
  this.undo.installController(aControllerWindow);

  this._containers = new Map();

  this._boundMutationObserver = this._mutationObserver.bind(this);
  this.walker.on("mutations", this._boundMutationObserver);

  this._boundOnNewSelection = this._onNewSelection.bind(this);
  this._inspector.selection.on("new-node-front", this._boundOnNewSelection);
  this._onNewSelection();

  this._boundKeyDown = this._onKeyDown.bind(this);
  this._frame.contentWindow.addEventListener("keydown", this._boundKeyDown, false);

  this._boundFocus = this._onFocus.bind(this);
  this._frame.addEventListener("focus", this._boundFocus, false);

  this._initPreview();
  this._initTooltips();
  this._initHighlighter();

  EventEmitter.decorate(this);
}

exports.MarkupView = MarkupView;

MarkupView.prototype = {
  _selectedContainer: null,

  _initTooltips: function() {
    this.tooltip = new Tooltip(this._inspector.panelDoc);
    this.tooltip.startTogglingOnHover(this._elt,
      this._buildTooltipContent.bind(this));
  },

  _initHighlighter: function() {
    // Show the box model on markup-view mousemove
    this._onMouseMove = this._onMouseMove.bind(this);
    this._elt.addEventListener("mousemove", this._onMouseMove, false);
    this._onMouseLeave = this._onMouseLeave.bind(this);
    this._elt.addEventListener("mouseleave", this._onMouseLeave, false);

    // Show markup-containers as hovered on toolbox "picker-node-hovered" event
    // which happens when the "pick" button is pressed
    this._onToolboxPickerHover = (event, nodeFront) => {
      this.showNode(nodeFront, true).then(() => {
        this._showContainerAsHovered(nodeFront);
      });
    }
    this._inspector.toolbox.on("picker-node-hovered", this._onToolboxPickerHover);
  },

  _onMouseMove: function(event) {
    let target = event.target;

    // Search target for a markupContainer reference, if not found, walk up
    while (!target.container) {
      if (target.tagName.toLowerCase() === "body") {
        return;
      }
      target = target.parentNode;
    }

    let container = target.container;
    if (this._hoveredNode !== container.node) {
      if (container.node.nodeType !== Ci.nsIDOMNode.TEXT_NODE) {
        this._showBoxModel(container.node);
      } else {
        this._hideBoxModel();
      }
    }
    this._showContainerAsHovered(container.node);
  },

  _hoveredNode: null,
  _showContainerAsHovered: function(nodeFront) {
    if (this._hoveredNode !== nodeFront) {
      if (this._hoveredNode) {
        this._containers.get(this._hoveredNode).hovered = false;
      }
      this._containers.get(nodeFront).hovered = true;

      this._hoveredNode = nodeFront;
    }
  },

  _onMouseLeave: function() {
    this._hideBoxModel(true);
    if (this._hoveredNode) {
      this._containers.get(this._hoveredNode).hovered = false;
    }
    this._hoveredNode = null;
  },

  _showBoxModel: function(nodeFront, options={}) {
    this._inspector.toolbox.highlighterUtils.highlightNodeFront(nodeFront, options);
  },

  _hideBoxModel: function(forceHide) {
    return this._inspector.toolbox.highlighterUtils.unhighlight(forceHide);
  },

  _briefBoxModelTimer: null,
  _brieflyShowBoxModel: function(nodeFront, options) {
    let win = this._frame.contentWindow;

    if (this._briefBoxModelTimer) {
      win.clearTimeout(this._briefBoxModelTimer);
      this._briefBoxModelTimer = null;
    }

    this._showBoxModel(nodeFront, options);

    this._briefBoxModelTimer = this._frame.contentWindow.setTimeout(() => {
      this._hideBoxModel();
    }, NEW_SELECTION_HIGHLIGHTER_TIMER);
  },

  template: function(aName, aDest, aOptions={stack: "markup-view.xhtml"}) {
    let node = this.doc.getElementById("template-" + aName).cloneNode(true);
    node.removeAttribute("id");
    template(node, aDest, aOptions);
    return node;
  },

  /**
   * Get the MarkupContainer object for a given node, or undefined if
   * none exists.
   */
  getContainer: function(aNode) {
    return this._containers.get(aNode);
  },

  update: function() {
    let updateChildren = function(node) {
      this.getContainer(node).update();
      for (let child of node.treeChildren()) {
        updateChildren(child);
      }
    }.bind(this);

    // Start with the documentElement
    let documentElement;
    for (let node of this._rootNode.treeChildren()) {
      if (node.isDocumentElement === true) {
        documentElement = node;
        break;
      }
    }

    // Recursively update each node starting with documentElement.
    updateChildren(documentElement);
  },

  _buildTooltipContent: function(target) {
    // From the target passed here, let's find the parent MarkupContainer
    // and ask it if the tooltip should be shown
    let parent = target, container;
    while (parent !== this.doc.body) {
      if (parent.container) {
        container = parent.container;
        break;
      }
      parent = parent.parentNode;
    }

    if (container) {
      // With the newly found container, delegate the tooltip content creation
      // and decision to show or not the tooltip
      return container._buildTooltipContent(target, this.tooltip);
    }
  },

  /**
   * Given the known reason, should the current selection be briefly highlighted
   * In a few cases, we don't want to highlight the node:
   * - If the reason is null (used to reset the selection),
   * - if it's "inspector-open" (when the inspector opens up, let's not highlight
   * the default node)
   * - if it's "navigateaway" (since the page is being navigated away from)
   * - if it's "test" (this is a special case for mochitest. In tests, we often
   * need to select elements but don't necessarily want the highlighter to come
   * and go after a delay as this might break test scenarios)
   * We also do not want to start a brief highlight timeout if the node is already
   * being hovered over, since in that case it will already be highlighted.
   */
  _shouldNewSelectionBeHighlighted: function() {
    let reason = this._inspector.selection.reason;
    let unwantedReasons = ["inspector-open", "navigateaway", "test"];
    let isHighlitNode = this._hoveredNode === this._inspector.selection.nodeFront;
    return !isHighlitNode && reason && unwantedReasons.indexOf(reason) === -1;
  },

  /**
   * Highlight the inspector selected node.
   */
  _onNewSelection: function() {
    let selection = this._inspector.selection;

    this.htmlEditor.hide();
    let done = this._inspector.updating("markup-view");
    if (selection.isNode()) {
      if (this._shouldNewSelectionBeHighlighted()) {
        this._brieflyShowBoxModel(selection.nodeFront, {
          scrollIntoView: true
        });
      }

      this.showNode(selection.nodeFront, true).then(() => {
        if (selection.reason !== "treepanel") {
          this.markNodeAsSelected(selection.nodeFront);
        }
        done();
      });
    } else {
      this.unmarkSelectedNode();
      done();
    }
  },

  /**
   * Create a TreeWalker to find the next/previous
   * node for selection.
   */
  _selectionWalker: function(aStart) {
    let walker = this.doc.createTreeWalker(
      aStart || this._elt,
      Ci.nsIDOMNodeFilter.SHOW_ELEMENT,
      function(aElement) {
        if (aElement.container &&
            aElement.container.elt === aElement &&
            aElement.container.visible) {
          return Ci.nsIDOMNodeFilter.FILTER_ACCEPT;
        }
        return Ci.nsIDOMNodeFilter.FILTER_SKIP;
      }
    );
    walker.currentNode = this._selectedContainer.elt;
    return walker;
  },

  /**
   * Key handling.
   */
  _onKeyDown: function(aEvent) {
    let handled = true;

    // Ignore keystrokes that originated in editors.
    if (aEvent.target.tagName.toLowerCase() === "input" ||
        aEvent.target.tagName.toLowerCase() === "textarea") {
      return;
    }

    switch(aEvent.keyCode) {
      case Ci.nsIDOMKeyEvent.DOM_VK_H:
        let node = this._selectedContainer.node;
        if (node.hidden) {
          this.walker.unhideNode(node).then(() => this.nodeChanged(node));
        } else {
          this.walker.hideNode(node).then(() => this.nodeChanged(node));
        }
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_DELETE:
      case Ci.nsIDOMKeyEvent.DOM_VK_BACK_SPACE:
        this.deleteNode(this._selectedContainer.node);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_HOME:
        let rootContainer = this._containers.get(this._rootNode);
        this.navigate(rootContainer.children.firstChild.container);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_LEFT:
        if (this._selectedContainer.expanded) {
          this.collapseNode(this._selectedContainer.node);
        } else {
          let parent = this._selectionWalker().parentNode();
          if (parent) {
            this.navigate(parent.container);
          }
        }
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_RIGHT:
        if (!this._selectedContainer.expanded &&
            this._selectedContainer.hasChildren) {
          this._expandContainer(this._selectedContainer);
        } else {
          let next = this._selectionWalker().nextNode();
          if (next) {
            this.navigate(next.container);
          }
        }
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_UP:
        let prev = this._selectionWalker().previousNode();
        if (prev) {
          this.navigate(prev.container);
        }
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_DOWN:
        let next = this._selectionWalker().nextNode();
        if (next) {
          this.navigate(next.container);
        }
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_PAGE_UP: {
        let walker = this._selectionWalker();
        let selection = this._selectedContainer;
        for (let i = 0; i < PAGE_SIZE; i++) {
          let prev = walker.previousNode();
          if (!prev) {
            break;
          }
          selection = prev.container;
        }
        this.navigate(selection);
        break;
      }
      case Ci.nsIDOMKeyEvent.DOM_VK_PAGE_DOWN: {
        let walker = this._selectionWalker();
        let selection = this._selectedContainer;
        for (let i = 0; i < PAGE_SIZE; i++) {
          let next = walker.nextNode();
          if (!next) {
            break;
          }
          selection = next.container;
        }
        this.navigate(selection);
        break;
      }
      case Ci.nsIDOMKeyEvent.DOM_VK_F2: {
        this.beginEditingOuterHTML(this._selectedContainer.node);
        break;
      }
      default:
        handled = false;
    }
    if (handled) {
      aEvent.stopPropagation();
      aEvent.preventDefault();
    }
  },

  /**
   * Delete a node from the DOM.
   * This is an undoable action.
   */
  deleteNode: function(aNode) {
    if (aNode.isDocumentElement ||
        aNode.nodeType == Ci.nsIDOMNode.DOCUMENT_TYPE_NODE) {
      return;
    }

    let container = this._containers.get(aNode);

    // Retain the node so we can undo this...
    this.walker.retainNode(aNode).then(() => {
      let parent = aNode.parentNode();
      let sibling = null;
      this.undo.do(() => {
        if (container.selected) {
          this.navigate(this._containers.get(parent));
        }
        this.walker.removeNode(aNode).then(nextSibling => {
          sibling = nextSibling;
        });
      }, () => {
        this.walker.insertBefore(aNode, parent, sibling);
      });
    }).then(null, console.error);
  },

  /**
   * If an editable item is focused, select its container.
   */
  _onFocus: function(aEvent) {
    let parent = aEvent.target;
    while (!parent.container) {
      parent = parent.parentNode;
    }
    if (parent) {
      this.navigate(parent.container, true);
    }
  },

  /**
   * Handle a user-requested navigation to a given MarkupContainer,
   * updating the inspector's currently-selected node.
   *
   * @param MarkupContainer aContainer
   *        The container we're navigating to.
   * @param aIgnoreFocus aIgnoreFocus
   *        If falsy, keyboard focus will be moved to the container too.
   */
  navigate: function(aContainer, aIgnoreFocus) {
    if (!aContainer) {
      return;
    }

    let node = aContainer.node;
    this.markNodeAsSelected(node, "treepanel");

    if (!aIgnoreFocus) {
      aContainer.focus();
    }
  },

  /**
   * Make sure a node is included in the markup tool.
   *
   * @param DOMNode aNode
   *        The node in the content document.
   * @param boolean aFlashNode
   *        Whether the newly imported node should be flashed
   * @returns MarkupContainer The MarkupContainer object for this element.
   */
  importNode: function(aNode, aFlashNode) {
    if (!aNode) {
      return null;
    }

    if (this._containers.has(aNode)) {
      return this._containers.get(aNode);
    }

    if (aNode === this.walker.rootNode) {
      var container = new RootContainer(this, aNode);
      this._elt.appendChild(container.elt);
      this._rootNode = aNode;
    } else {
      var container = new MarkupContainer(this, aNode, this._inspector);
      if (aFlashNode) {
        container.flashMutation();
      }
    }

    this._containers.set(aNode, container);
    container.childrenDirty = true;

    this._updateChildren(container);

    return container;
  },

  /**
   * Mutation observer used for included nodes.
   */
  _mutationObserver: function(aMutations) {
    let requiresLayoutChange = false;
    let reselectParent;
    let reselectChildIndex;

    for (let mutation of aMutations) {
      let type = mutation.type;
      let target = mutation.target;

      if (mutation.type === "documentUnload") {
        // Treat this as a childList change of the child (maybe the protocol
        // should do this).
        type = "childList";
        target = mutation.targetParent;
        if (!target) {
          continue;
        }
      }

      let container = this._containers.get(target);
      if (!container) {
        // Container might not exist if this came from a load event for a node
        // we're not viewing.
        continue;
      }
      if (type === "attributes" || type === "characterData") {
        container.update();

        // Auto refresh style properties on selected node when they change.
        if (type === "attributes" && container.selected) {
          requiresLayoutChange = true;
        }
      } else if (type === "childList") {
        let isFromOuterHTML = mutation.removed.some((n) => {
          return n === this._outerHTMLNode;
        });

        // Keep track of which node should be reselected after mutations.
        if (isFromOuterHTML) {
          reselectParent = target;
          reselectChildIndex = this._outerHTMLChildIndex;

          delete this._outerHTMLNode;
          delete this._outerHTMLChildIndex;
        }

        container.childrenDirty = true;
        // Update the children to take care of changes in the markup view DOM.
        this._updateChildren(container, {flash: !isFromOuterHTML});
      }
    }

    if (requiresLayoutChange) {
      this._inspector.immediateLayoutChange();
    }
    this._waitForChildren().then((nodes) => {
      this._flashMutatedNodes(aMutations);
      this._inspector.emit("markupmutation", aMutations);

      // Since the htmlEditor is absolutely positioned, a mutation may change
      // the location in which it should be shown.
      this.htmlEditor.refresh();

      // If a node has had its outerHTML set, the parent node will be selected.
      // Reselect the original node immediately.
      if (this._inspector.selection.nodeFront === reselectParent) {
        this.walker.children(reselectParent).then((o) => {
          let node = o.nodes[reselectChildIndex];
          let container = this._containers.get(node);
          if (node && container) {
            this.markNodeAsSelected(node, "outerhtml");
            if (container.hasChildren) {
              this.expandNode(node);
            }
          }
        });

      }
    });
  },

  /**
   * Given a list of mutations returned by the mutation observer, flash the
   * corresponding containers to attract attention.
   */
  _flashMutatedNodes: function(aMutations) {
    let addedOrEditedContainers = new Set();
    let removedContainers = new Set();

    for (let {type, target, added, removed} of aMutations) {
      let container = this._containers.get(target);

      if (container) {
        if (type === "attributes" || type === "characterData") {
          addedOrEditedContainers.add(container);
        } else if (type === "childList") {
          // If there has been removals, flash the parent
          if (removed.length) {
            removedContainers.add(container);
          }

          // If there has been additions, flash the nodes
          added.forEach(added => {
            let addedContainer = this._containers.get(added);
            addedOrEditedContainers.add(addedContainer);

            // The node may be added as a result of an append, in which case it
            // it will have been removed from another container first, but in
            // these cases we don't want to flash both the removal and the
            // addition
            removedContainers.delete(container);
          });
        }
      }
    }

    for (let container of removedContainers) {
      container.flashMutation();
    }
    for (let container of addedOrEditedContainers) {
      container.flashMutation();
    }
  },

  /**
   * Make sure the given node's parents are expanded and the
   * node is scrolled on to screen.
   */
  showNode: function(aNode, centered) {
    let parent = aNode;

    this.importNode(aNode);

    while ((parent = parent.parentNode())) {
      this.importNode(parent);
      this.expandNode(parent);
    }

    return this._waitForChildren().then(() => {
      return this._ensureVisible(aNode);
    }).then(() => {
      // Why is this not working?
      this.layoutHelpers.scrollIntoViewIfNeeded(this._containers.get(aNode).editor.elt, centered);
    });
  },

  /**
   * Expand the container's children.
   */
  _expandContainer: function(aContainer) {
    return this._updateChildren(aContainer, {expand: true}).then(() => {
      aContainer.expanded = true;
    });
  },

  /**
   * Expand the node's children.
   */
  expandNode: function(aNode) {
    let container = this._containers.get(aNode);
    this._expandContainer(container);
  },

  /**
   * Expand the entire tree beneath a container.
   *
   * @param aContainer The container to expand.
   */
  _expandAll: function(aContainer) {
    return this._expandContainer(aContainer).then(() => {
      let child = aContainer.children.firstChild;
      let promises = [];
      while (child) {
        promises.push(this._expandAll(child.container));
        child = child.nextSibling;
      }
      return promise.all(promises);
    }).then(null, console.error);
  },

  /**
   * Expand the entire tree beneath a node.
   *
   * @param aContainer The node to expand, or null
   *        to start from the top.
   */
  expandAll: function(aNode) {
    aNode = aNode || this._rootNode;
    return this._expandAll(this._containers.get(aNode));
  },

  /**
   * Collapse the node's children.
   */
  collapseNode: function(aNode) {
    let container = this._containers.get(aNode);
    container.expanded = false;
  },

  /**
   * Retrieve the outerHTML for a remote node.
   * @param aNode The NodeFront to get the outerHTML for.
   * @returns A promise that will be resolved with the outerHTML.
   */
  getNodeOuterHTML: function(aNode) {
    let def = promise.defer();
    this.walker.outerHTML(aNode).then(longstr => {
      longstr.string().then(outerHTML => {
        longstr.release().then(null, console.error);
        def.resolve(outerHTML);
      });
    });
    return def.promise;
  },

  /**
   * Retrieve the index of a child within its parent's children list.
   * @param aNode The NodeFront to find the index of.
   * @returns A promise that will be resolved with the integer index.
   *          If the child cannot be found, returns -1
   */
  getNodeChildIndex: function(aNode) {
    let def = promise.defer();
    let parentNode = aNode.parentNode();

    // Node may have been removed from the DOM, instead of throwing an error,
    // return -1 indicating that it isn't inside of its parent children list.
    if (!parentNode) {
      def.resolve(-1);
    } else {
      this.walker.children(parentNode).then(children => {
        def.resolve(children.nodes.indexOf(aNode));
      });
    }

    return def.promise;
  },

  /**
   * Retrieve the index of a child within its parent's children collection.
   * @param aNode The NodeFront to find the index of.
   * @param newValue The new outerHTML to set on the node.
   * @param oldValue The old outerHTML that will be reverted to find the index of.
   * @returns A promise that will be resolved with the integer index.
   *          If the child cannot be found, returns -1
   */
  updateNodeOuterHTML: function(aNode, newValue, oldValue) {
    let container = this._containers.get(aNode);
    if (!container) {
      return;
    }

    this.getNodeChildIndex(aNode).then((i) => {
      this._outerHTMLChildIndex = i;
      this._outerHTMLNode = aNode;

      container.undo.do(() => {
        this.walker.setOuterHTML(aNode, newValue);
      }, () => {
        this.walker.setOuterHTML(aNode, oldValue);
      });
    });
  },

  /**
   * Open an editor in the UI to allow editing of a node's outerHTML.
   * @param aNode The NodeFront to edit.
   */
  beginEditingOuterHTML: function(aNode) {
    this.getNodeOuterHTML(aNode).then((oldValue)=> {
      let container = this._containers.get(aNode);
      if (!container) {
        return;
      }
      this.htmlEditor.show(container.tagLine, oldValue);
      this.htmlEditor.once("popuphidden", (e, aCommit, aValue) => {
        // Need to focus the <html> element instead of the frame / window
        // in order to give keyboard focus back to doc (from editor).
        this._frame.contentDocument.documentElement.focus();

        if (aCommit) {
          this.updateNodeOuterHTML(aNode, aValue, oldValue);
        }
      });
    });
  },

  /**
   * Mark the given node expanded.
   * @param aNode The NodeFront to mark as expanded.
   */
  setNodeExpanded: function(aNode, aExpanded) {
    if (aExpanded) {
      this.expandNode(aNode);
    } else {
      this.collapseNode(aNode);
    }
  },

  /**
   * Mark the given node selected, and update the inspector.selection
   * object's NodeFront to keep consistent state between UI and selection.
   * @param aNode The NodeFront to mark as selected.
   */
  markNodeAsSelected: function(aNode, reason) {
    let container = this._containers.get(aNode);
    if (this._selectedContainer === container) {
      return false;
    }
    if (this._selectedContainer) {
      this._selectedContainer.selected = false;
    }
    this._selectedContainer = container;
    if (aNode) {
      this._selectedContainer.selected = true;
    }

    this._inspector.selection.setNodeFront(aNode, reason || "nodeselected");
    return true;
  },

  /**
   * Make sure that every ancestor of the selection are updated
   * and included in the list of visible children.
   */
  _ensureVisible: function(node) {
    while (node) {
      let container = this._containers.get(node);
      let parent = node.parentNode();
      if (!container.elt.parentNode) {
        let parentContainer = this._containers.get(parent);
        parentContainer.childrenDirty = true;
        this._updateChildren(parentContainer, {expand: node});
      }

      node = parent;
    }
    return this._waitForChildren();
  },

  /**
   * Unmark selected node (no node selected).
   */
  unmarkSelectedNode: function() {
    if (this._selectedContainer) {
      this._selectedContainer.selected = false;
      this._selectedContainer = null;
    }
  },

  /**
   * Called when the markup panel initiates a change on a node.
   */
  nodeChanged: function(aNode) {
    if (aNode === this._inspector.selection.nodeFront) {
      this._inspector.change("markupview");
    }
  },

  /**
   * Check if the current selection is a descendent of the container.
   * if so, make sure it's among the visible set for the container,
   * and set the dirty flag if needed.
   * @returns The node that should be made visible, if any.
   */
  _checkSelectionVisible: function(aContainer) {
    let centered = null;
    let node = this._inspector.selection.nodeFront;
    while (node) {
      if (node.parentNode() === aContainer.node) {
        centered = node;
        break;
      }
      node = node.parentNode();
    }

    return centered;
  },

  /**
   * Make sure all children of the given container's node are
   * imported and attached to the container in the right order.
   *
   * Children need to be updated only in the following circumstances:
   * a) We just imported this node and have never seen its children.
   *    container.childrenDirty will be set by importNode in this case.
   * b) We received a childList mutation on the node.
   *    container.childrenDirty will be set in that case too.
   * c) We have changed the selection, and the path to that selection
   *    wasn't loaded in a previous children request (because we only
   *    grab a subset).
   *    container.childrenDirty should be set in that case too!
   *
   * @param MarkupContainer aContainer
   *        The markup container whose children need updating
   * @param Object options
   *        Options are {expand:boolean,flash:boolean}
   * @return a promise that will be resolved when the children are ready
   * (which may be immediately).
   */
  _updateChildren: function(aContainer, options) {
    let expand = options && options.expand;
    let flash = options && options.flash;

    aContainer.hasChildren = aContainer.node.hasChildren;

    if (!this._queuedChildUpdates) {
      this._queuedChildUpdates = new Map();
    }

    if (this._queuedChildUpdates.has(aContainer)) {
      return this._queuedChildUpdates.get(aContainer);
    }

    if (!aContainer.childrenDirty) {
      return promise.resolve(aContainer);
    }

    if (!aContainer.hasChildren) {
      while (aContainer.children.firstChild) {
        aContainer.children.removeChild(aContainer.children.firstChild);
      }
      aContainer.childrenDirty = false;
      return promise.resolve(aContainer);
    }

    // If we're not expanded (or asked to update anyway), we're done for
    // now.  Note that this will leave the childrenDirty flag set, so when
    // expanded we'll refresh the child list.
    if (!(aContainer.expanded || expand)) {
      return promise.resolve(aContainer);
    }

    // We're going to issue a children request, make sure it includes the
    // centered node.
    let centered = this._checkSelectionVisible(aContainer);

    // Children aren't updated yet, but clear the childrenDirty flag anyway.
    // If the dirty flag is re-set while we're fetching we'll need to fetch
    // again.
    aContainer.childrenDirty = false;
    let updatePromise = this._getVisibleChildren(aContainer, centered).then(children => {
      if (!this._containers) {
        return promise.reject("markup view destroyed");
      }
      this._queuedChildUpdates.delete(aContainer);

      // If children are dirty, we got a change notification for this node
      // while the request was in progress, we need to do it again.
      if (aContainer.childrenDirty) {
        return this._updateChildren(aContainer, {expand: centered});
      }

      let fragment = this.doc.createDocumentFragment();

      for (let child of children.nodes) {
        let container = this.importNode(child, flash);
        fragment.appendChild(container.elt);
      }

      while (aContainer.children.firstChild) {
        aContainer.children.removeChild(aContainer.children.firstChild);
      }

      if (!(children.hasFirst && children.hasLast)) {
        let data = {
          showing: this.strings.GetStringFromName("markupView.more.showing"),
          showAll: this.strings.formatStringFromName(
                    "markupView.more.showAll",
                    [aContainer.node.numChildren.toString()], 1),
          allButtonClick: () => {
            aContainer.maxChildren = -1;
            aContainer.childrenDirty = true;
            this._updateChildren(aContainer);
          }
        };

        if (!children.hasFirst) {
          let span = this.template("more-nodes", data);
          fragment.insertBefore(span, fragment.firstChild);
        }
        if (!children.hasLast) {
          let span = this.template("more-nodes", data);
          fragment.appendChild(span);
        }
      }

      aContainer.children.appendChild(fragment);
      return aContainer;
    }).then(null, console.error);
    this._queuedChildUpdates.set(aContainer, updatePromise);
    return updatePromise;
  },

  _waitForChildren: function() {
    if (!this._queuedChildUpdates) {
      return promise.resolve(undefined);
    }
    return promise.all([updatePromise for (updatePromise of this._queuedChildUpdates.values())]);
  },

  /**
   * Return a list of the children to display for this container.
   */
  _getVisibleChildren: function(aContainer, aCentered) {
    let maxChildren = aContainer.maxChildren || this.maxChildren;
    if (maxChildren == -1) {
      maxChildren = undefined;
    }

    return this.walker.children(aContainer.node, {
      maxNodes: maxChildren,
      center: aCentered
    });
  },

  /**
   * Tear down the markup panel.
   */
  destroy: function() {
    if (this._destroyer) {
      return this._destroyer;
    }

    // Note that if the toolbox is closed, this will work fine, but will fail
    // in case the browser is closed and will trigger a noSuchActor message.
    this._destroyer = this._hideBoxModel();

    this._hoveredNode = null;
    this._inspector.toolbox.off("picker-node-hovered", this._onToolboxPickerHover);

    this.htmlEditor.destroy();
    this.htmlEditor = null;

    this.undo.destroy();
    this.undo = null;

    this.popup.destroy();
    this.popup = null;

    this._frame.removeEventListener("focus", this._boundFocus, false);
    this._boundFocus = null;

    if (this._boundUpdatePreview) {
      this._frame.contentWindow.removeEventListener("scroll",
        this._boundUpdatePreview, true);
      this._boundUpdatePreview = null;
    }

    if (this._boundResizePreview) {
      this._frame.contentWindow.removeEventListener("resize",
        this._boundResizePreview, true);
      this._frame.contentWindow.removeEventListener("overflow",
        this._boundResizePreview, true);
      this._frame.contentWindow.removeEventListener("underflow",
        this._boundResizePreview, true);
      this._boundResizePreview = null;
    }

    this._frame.contentWindow.removeEventListener("keydown",
      this._boundKeyDown, false);
    this._boundKeyDown = null;

    this._inspector.selection.off("new-node-front", this._boundOnNewSelection);
    this._boundOnNewSelection = null;

    this.walker.off("mutations", this._boundMutationObserver)
    this._boundMutationObserver = null;

    this._elt.removeEventListener("mousemove", this._onMouseMove, false);
    this._elt.removeEventListener("mouseleave", this._onMouseLeave, false);
    this._elt = null;

    for (let [key, container] of this._containers) {
      container.destroy();
    }
    this._containers = null;

    this.tooltip.destroy();
    this.tooltip = null;

    return this._destroyer;
  },

  /**
   * Initialize the preview panel.
   */
  _initPreview: function() {
    this._previewEnabled = Services.prefs.getBoolPref("devtools.inspector.markupPreview");
    if (!this._previewEnabled) {
      return;
    }

    this._previewBar = this.doc.querySelector("#previewbar");
    this._preview = this.doc.querySelector("#preview");
    this._viewbox = this.doc.querySelector("#viewbox");

    this._previewBar.classList.remove("disabled");

    this._previewWidth = this._preview.getBoundingClientRect().width;

    this._boundResizePreview = this._resizePreview.bind(this);
    this._frame.contentWindow.addEventListener("resize",
      this._boundResizePreview, true);
    this._frame.contentWindow.addEventListener("overflow",
      this._boundResizePreview, true);
    this._frame.contentWindow.addEventListener("underflow",
      this._boundResizePreview, true);

    this._boundUpdatePreview = this._updatePreview.bind(this);
    this._frame.contentWindow.addEventListener("scroll",
      this._boundUpdatePreview, true);
    this._updatePreview();
  },

  /**
   * Move the preview viewbox.
   */
  _updatePreview: function() {
    if (!this._previewEnabled) {
      return;
    }
    let win = this._frame.contentWindow;

    if (win.scrollMaxY == 0) {
      this._previewBar.classList.add("disabled");
      return;
    }

    this._previewBar.classList.remove("disabled");

    let ratio = this._previewWidth / PREVIEW_AREA;
    let width = ratio * win.innerWidth;

    let height = ratio * (win.scrollMaxY + win.innerHeight);
    let scrollTo
    if (height >= win.innerHeight) {
      scrollTo = -(height - win.innerHeight) * (win.scrollY / win.scrollMaxY);
      this._previewBar.setAttribute("style", "height:" + height +
        "px;transform:translateY(" + scrollTo + "px)");
    } else {
      this._previewBar.setAttribute("style", "height:100%");
    }

    let bgSize = ~~width + "px " + ~~height + "px";
    this._preview.setAttribute("style", "background-size:" + bgSize);

    let height = ~~(win.innerHeight * ratio) + "px";
    let top = ~~(win.scrollY * ratio) + "px";
    this._viewbox.setAttribute("style", "height:" + height +
      ";transform: translateY(" + top + ")");
  },

  /**
   * Hide the preview while resizing, to avoid slowness.
   */
  _resizePreview: function() {
    if (!this._previewEnabled) {
      return;
    }
    let win = this._frame.contentWindow;
    this._previewBar.classList.add("hide");
    win.clearTimeout(this._resizePreviewTimeout);

    win.setTimeout(function() {
      this._updatePreview();
      this._previewBar.classList.remove("hide");
    }.bind(this), 1000);
  }
};


/**
 * The main structure for storing a document node in the markup
 * tree.  Manages creation of the editor for the node and
 * a <ul> for placing child elements, and expansion/collapsing
 * of the element.
 *
 * @param MarkupView aMarkupView
 *        The markup view that owns this container.
 * @param DOMNode aNode
 *        The node to display.
 * @param Inspector aInspector
 *        The inspector tool container the markup-view
 */
function MarkupContainer(aMarkupView, aNode, aInspector) {
  this.markup = aMarkupView;
  this.doc = this.markup.doc;
  this.undo = this.markup.undo;
  this.node = aNode;
  this._inspector = aInspector;

  if (aNode.nodeType == Ci.nsIDOMNode.TEXT_NODE) {
    this.editor = new TextEditor(this, aNode, "text");
  } else if (aNode.nodeType == Ci.nsIDOMNode.COMMENT_NODE) {
    this.editor = new TextEditor(this, aNode, "comment");
  } else if (aNode.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
    this.editor = new ElementEditor(this, aNode);
  } else if (aNode.nodeType == Ci.nsIDOMNode.DOCUMENT_TYPE_NODE) {
    this.editor = new DoctypeEditor(this, aNode);
  } else {
    this.editor = new GenericEditor(this, aNode);
  }

  // The template will fill the following properties
  this.elt = null;
  this.expander = null;
  this.tagState = null;
  this.tagLine = null;
  this.children = null;
  this.markup.template("container", this);
  this.elt.container = this;
  this.children.container = this;

  // Expanding/collapsing the node on dblclick of the whole tag-line element
  this._onToggle = this._onToggle.bind(this);
  this.elt.addEventListener("dblclick", this._onToggle, false);
  this.expander.addEventListener("click", this._onToggle, false);

  // Appending the editor element and attaching event listeners
  this.tagLine.appendChild(this.editor.elt);

  this._onMouseDown = this._onMouseDown.bind(this);
  this.elt.addEventListener("mousedown", this._onMouseDown, false);

  // Prepare the image preview tooltip data if any
  this._prepareImagePreview();
}

MarkupContainer.prototype = {
  toString: function() {
    return "[MarkupContainer for " + this.node + "]";
  },

  isPreviewable: function() {
    if (this.node.tagName) {
      let tagName = this.node.tagName.toLowerCase();
      let srcAttr = this.editor.getAttributeElement("src");
      let isImage = tagName === "img" && srcAttr;
      let isCanvas = tagName === "canvas";

      return isImage || isCanvas;
    } else {
      return false;
    }
  },

  _prepareImagePreview: function() {
    if (this.isPreviewable()) {
      // Get the image data for later so that when the user actually hovers over
      // the element, the tooltip does contain the image
      let def = promise.defer();

      this.tooltipData = {
        target: this.editor.getAttributeElement("src") || this.editor.tag,
        data: def.promise
      };

      let maxDim = Services.prefs.getIntPref("devtools.inspector.imagePreviewTooltipSize");
      this.node.getImageData(maxDim).then(data => {
        data.data.string().then(str => {
          let res = {data: str, size: data.size};
          // Resolving the data promise and, to always keep tooltipData.data
          // as a promise, create a new one that resolves immediately
          def.resolve(res);
          this.tooltipData.data = promise.resolve(res);
        });
      }, () => {
        this.tooltipData.data = promise.reject();
      });
    }
  },

  copyImageDataUri: function() {
    // We need to send again a request to gettooltipData even if one was sent for
    // the tooltip, because we want the full-size image
    this.node.getImageData().then(data => {
      data.data.string().then(str => {
        clipboardHelper.copyString(str, this.markup.doc);
      });
    });
  },

  _buildTooltipContent: function(target, tooltip) {
    if (this.tooltipData && target === this.tooltipData.target) {
      this.tooltipData.data.then(({data, size}) => {
        tooltip.setImageContent(data, size);
      }, () => {
        tooltip.setBrokenImageContent();
      });
      return true;
    }
  },

  /**
   * True if the current node has children.  The MarkupView
   * will set this attribute for the MarkupContainer.
   */
  _hasChildren: false,

  get hasChildren() {
    return this._hasChildren;
  },

  set hasChildren(aValue) {
    this._hasChildren = aValue;
    if (aValue) {
      this.expander.style.visibility = "visible";
    } else {
      this.expander.style.visibility = "hidden";
    }
  },

  parentContainer: function() {
    return this.elt.parentNode ? this.elt.parentNode.container : null;
  },

  /**
   * True if the node has been visually expanded in the tree.
   */
  get expanded() {
    return !this.elt.classList.contains("collapsed");
  },

  set expanded(aValue) {
    if (aValue && this.elt.classList.contains("collapsed")) {
      // Expanding a node means cloning its "inline" closing tag into a new
      // tag-line that the user can interact with and showing the children.
      if (this.editor instanceof ElementEditor) {
        let closingTag = this.elt.querySelector(".close");
        if (closingTag) {
          if (!this.closeTagLine) {
            let line = this.markup.doc.createElement("div");
            line.classList.add("tag-line");

            let tagState = this.markup.doc.createElement("div");
            tagState.classList.add("tag-state");
            line.appendChild(tagState);

            line.appendChild(closingTag.cloneNode(true));

            this.closeTagLine = line;
          }
          this.elt.appendChild(this.closeTagLine);
        }
      }
      this.elt.classList.remove("collapsed");
      this.expander.setAttribute("open", "");
      this.hovered = false;
    } else if (!aValue) {
      if (this.editor instanceof ElementEditor && this.closeTagLine) {
        this.elt.removeChild(this.closeTagLine);
      }
      this.elt.classList.add("collapsed");
      this.expander.removeAttribute("open");
    }
  },

  _onToggle: function(event) {
    this.markup.navigate(this);
    if(this.hasChildren) {
      this.markup.setNodeExpanded(this.node, !this.expanded);
    }
    event.stopPropagation();
  },

  _onMouseDown: function(event) {
    let target = event.target;

    // Target may be a resource link (generated by the output-parser)
    if (target.nodeName === "a") {
      event.stopPropagation();
      event.preventDefault();
      let browserWin = this.markup._inspector.target
                           .tab.ownerDocument.defaultView;
      browserWin.openUILinkIn(target.href, "tab");
    }
    // Or it may be the "show more nodes" button (which already has its onclick)
    // Else, it's the container itself
    else if (target.nodeName !== "button") {
      this.hovered = false;
      this.markup.navigate(this);
      event.stopPropagation();
    }
  },

  /**
   * Temporarily flash the container to attract attention.
   * Used for markup mutations.
   */
  flashMutation: function() {
    if (!this.selected) {
      let contentWin = this.markup._frame.contentWindow;
      this.flashed = true;
      if (this._flashMutationTimer) {
        contentWin.clearTimeout(this._flashMutationTimer);
        this._flashMutationTimer = null;
      }
      this._flashMutationTimer = contentWin.setTimeout(() => {
        this.flashed = false;
      }, CONTAINER_FLASHING_DURATION);
    }
  },

  set flashed(aValue) {
    if (aValue) {
      // Make sure the animation class is not here
      this.tagState.classList.remove("flash-out");

      // Change the background
      this.tagState.classList.add("theme-bg-contrast");

      // Change the text color
      this.editor.elt.classList.add("theme-fg-contrast");
      [].forEach.call(
        this.editor.elt.querySelectorAll("[class*=theme-fg-color]"),
        span => span.classList.add("theme-fg-contrast")
      );
    } else {
      // Add the animation class to smoothly remove the background
      this.tagState.classList.add("flash-out");

      // Remove the background
      this.tagState.classList.remove("theme-bg-contrast");

      // Remove the text color
      this.editor.elt.classList.remove("theme-fg-contrast");
      [].forEach.call(
        this.editor.elt.querySelectorAll("[class*=theme-fg-color]"),
        span => span.classList.remove("theme-fg-contrast")
      );
    }
  },

  _hovered: false,

  /**
   * Highlight the currently hovered tag + its closing tag if necessary
   * (that is if the tag is expanded)
   */
  set hovered(aValue) {
    this.tagState.classList.remove("flash-out");
    this._hovered = aValue;
    if (aValue) {
      if (!this.selected) {
        this.tagState.classList.add("theme-bg-darker");
      }
      if (this.closeTagLine) {
        this.closeTagLine.querySelector(".tag-state").classList.add(
          "theme-bg-darker");
      }
    } else {
      this.tagState.classList.remove("theme-bg-darker");
      if (this.closeTagLine) {
        this.closeTagLine.querySelector(".tag-state").classList.remove(
          "theme-bg-darker");
      }
    }
  },

  /**
   * True if the container is visible in the markup tree.
   */
  get visible() {
    return this.elt.getBoundingClientRect().height > 0;
  },

  /**
   * True if the container is currently selected.
   */
  _selected: false,

  get selected() {
    return this._selected;
  },

  set selected(aValue) {
    this.tagState.classList.remove("flash-out");
    this._selected = aValue;
    this.editor.selected = aValue;
    if (this._selected) {
      this.tagLine.setAttribute("selected", "");
      this.tagState.classList.add("theme-selected");
    } else {
      this.tagLine.removeAttribute("selected");
      this.tagState.classList.remove("theme-selected");
    }
  },

  /**
   * Update the container's editor to the current state of the
   * viewed node.
   */
  update: function() {
    if (this.editor.update) {
      this.editor.update();
    }
  },

  /**
   * Try to put keyboard focus on the current editor.
   */
  focus: function() {
    let focusable = this.editor.elt.querySelector("[tabindex]");
    if (focusable) {
      focusable.focus();
    }
  },

  /**
   * Get rid of event listeners and references, when the container is no longer
   * needed
   */
  destroy: function() {
    // Recursively destroy children containers
    let firstChild;
    while (firstChild = this.children.firstChild) {
      // Not all children of a container are containers themselves
      // ("show more nodes" button is one example)
      if (firstChild.container) {
        firstChild.container.destroy();
      }
      this.children.removeChild(firstChild);
    }

    // Remove event listeners
    this.elt.removeEventListener("dblclick", this._onToggle, false);
    this.elt.removeEventListener("mousedown", this._onMouseDown, false);
    this.expander.removeEventListener("click", this._onToggle, false);

    // Destroy my editor
    this.editor.destroy();
  }
};


/**
 * Dummy container node used for the root document element.
 */
function RootContainer(aMarkupView, aNode) {
  this.doc = aMarkupView.doc;
  this.elt = this.doc.createElement("ul");
  this.elt.container = this;
  this.children = this.elt;
  this.node = aNode;
  this.toString = () => "[root container]";
}

RootContainer.prototype = {
  hasChildren: true,
  expanded: true,
  update: function() {},
  destroy: function() {}
};

/**
 * Creates an editor for simple nodes.
 */
function GenericEditor(aContainer, aNode) {
  this.elt = aContainer.doc.createElement("span");
  this.elt.className = "editor";
  this.elt.textContent = aNode.nodeName;
}

GenericEditor.prototype = {
  destroy: function() {}
};

/**
 * Creates an editor for a DOCTYPE node.
 *
 * @param MarkupContainer aContainer The container owning this editor.
 * @param DOMNode aNode The node being edited.
 */
function DoctypeEditor(aContainer, aNode) {
  this.elt = aContainer.doc.createElement("span");
  this.elt.className = "editor comment";
  this.elt.textContent = '<!DOCTYPE ' + aNode.name +
     (aNode.publicId ? ' PUBLIC "' +  aNode.publicId + '"': '') +
     (aNode.systemId ? ' "' + aNode.systemId + '"' : '') +
     '>';
}

DoctypeEditor.prototype = {
  destroy: function() {}
};

/**
 * Creates a simple text editor node, used for TEXT and COMMENT
 * nodes.
 *
 * @param MarkupContainer aContainer The container owning this editor.
 * @param DOMNode aNode The node being edited.
 * @param string aTemplate The template id to use to build the editor.
 */
function TextEditor(aContainer, aNode, aTemplate) {
  this.node = aNode;
  this._selected = false;

  aContainer.markup.template(aTemplate, this);

  editableField({
    element: this.value,
    stopOnReturn: true,
    trigger: "dblclick",
    multiline: true,
    done: (aVal, aCommit) => {
      if (!aCommit) {
        return;
      }
      this.node.getNodeValue().then(longstr => {
        longstr.string().then(oldValue => {
          longstr.release().then(null, console.error);

          aContainer.undo.do(() => {
            this.node.setNodeValue(aVal).then(() => {
              aContainer.markup.nodeChanged(this.node);
            });
          }, () => {
            this.node.setNodeValue(oldValue).then(() => {
              aContainer.markup.nodeChanged(this.node);
            })
          });
        });
      });
    }
  });

  this.update();
}

TextEditor.prototype = {
  get selected() this._selected,
  set selected(aValue) {
    if (aValue === this._selected) {
      return;
    }
    this._selected = aValue;
    this.update();
  },

  update: function() {
    if (!this.selected || !this.node.incompleteValue) {
      let text = this.node.shortValue;
      // XXX: internationalize the elliding
      if (this.node.incompleteValue) {
        text += "…";
      }
      this.value.textContent = text;
    } else {
      let longstr = null;
      this.node.getNodeValue().then(ret => {
        longstr = ret;
        return longstr.string();
      }).then(str => {
        longstr.release().then(null, console.error);
        if (this.selected) {
          this.value.textContent = str;
        }
      }).then(null, console.error);
    }
  },

  destroy: function() {}
};

/**
 * Creates an editor for an Element node.
 *
 * @param MarkupContainer aContainer The container owning this editor.
 * @param Element aNode The node being edited.
 */
function ElementEditor(aContainer, aNode) {
  this.doc = aContainer.doc;
  this.undo = aContainer.undo;
  this.template = aContainer.markup.template.bind(aContainer.markup);
  this.container = aContainer;
  this.markup = this.container.markup;
  this.node = aNode;

  this.attrs = {};

  // The templates will fill the following properties
  this.elt = null;
  this.tag = null;
  this.closeTag = null;
  this.attrList = null;
  this.newAttr = null;
  this.closeElt = null;

  // Create the main editor
  this.template("element", this);

  if (aNode.isLocal_toBeDeprecated()) {
    this.rawNode = aNode.rawNode();
  }

  // Make the tag name editable (unless this is a remote node or
  // a document element)
  if (this.rawNode && !aNode.isDocumentElement) {
    this.tag.setAttribute("tabindex", "0");
    editableField({
      element: this.tag,
      trigger: "dblclick",
      stopOnReturn: true,
      done: this.onTagEdit.bind(this),
    });
  }

  // Make the new attribute space editable.
  editableField({
    element: this.newAttr,
    trigger: "dblclick",
    stopOnReturn: true,
    contentType: InplaceEditor.CONTENT_TYPES.CSS_MIXED,
    popup: this.markup.popup,
    done: (aVal, aCommit) => {
      if (!aCommit) {
        return;
      }

      try {
        let doMods = this._startModifyingAttributes();
        let undoMods = this._startModifyingAttributes();
        this._applyAttributes(aVal, null, doMods, undoMods);
        this.undo.do(() => {
          doMods.apply();
        }, function() {
          undoMods.apply();
        });
      } catch(x) {
        console.error(x);
      }
    }
  });

  let tagName = this.node.nodeName.toLowerCase();
  this.tag.textContent = tagName;
  this.closeTag.textContent = tagName;

  this.update();
}

ElementEditor.prototype = {
  /**
   * Update the state of the editor from the node.
   */
  update: function() {
    let attrs = this.node.attributes;
    if (!attrs) {
      return;
    }

    // Hide all the attribute editors, they'll be re-shown if they're
    // still applicable.  Don't update attributes that are being
    // actively edited.
    let attrEditors = this.attrList.querySelectorAll(".attreditor");
    for (let i = 0; i < attrEditors.length; i++) {
      if (!attrEditors[i].inplaceEditor) {
        attrEditors[i].style.display = "none";
      }
    }

    // Get the attribute editor for each attribute that exists on
    // the node and show it.
    for (let attr of attrs) {
      let attribute = this._createAttribute(attr);
      if (!attribute.inplaceEditor) {
        attribute.style.removeProperty("display");
      }
    }
  },

  _startModifyingAttributes: function() {
    return this.node.startModifyingAttributes();
  },

  /**
   * Get the element used for one of the attributes of this element
   * @param string attrName The name of the attribute to get the element for
   * @return DOMElement
   */
  getAttributeElement: function(attrName) {
    return this.attrList.querySelector(
      ".attreditor[data-attr=" + attrName + "] .attr-value");
  },

  _createAttribute: function(aAttr, aBefore = null) {
    // Create the template editor, which will save some variables here.
    let data = {
      attrName: aAttr.name,
    };
    this.template("attribute", data);
    var {attr, inner, name, val} = data;

    // Double quotes need to be handled specially to prevent DOMParser failing.
    // name="v"a"l"u"e" when editing -> name='v"a"l"u"e"'
    // name="v'a"l'u"e" when editing -> name="v'a&quot;l'u&quot;e"
    let editValueDisplayed = aAttr.value || "";
    let hasDoubleQuote = editValueDisplayed.contains('"');
    let hasSingleQuote = editValueDisplayed.contains("'");
    let initial = aAttr.name + '="' + editValueDisplayed + '"';

    // Can't just wrap value with ' since the value contains both " and '.
    if (hasDoubleQuote && hasSingleQuote) {
        editValueDisplayed = editValueDisplayed.replace(/\"/g, "&quot;");
        initial = aAttr.name + '="' + editValueDisplayed + '"';
    }

    // Wrap with ' since there are no single quotes in the attribute value.
    if (hasDoubleQuote && !hasSingleQuote) {
        initial = aAttr.name + "='" + editValueDisplayed + "'";
    }

    // Make the attribute editable.
    editableField({
      element: inner,
      trigger: "dblclick",
      stopOnReturn: true,
      selectAll: false,
      initial: initial,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_MIXED,
      popup: this.markup.popup,
      start: (aEditor, aEvent) => {
        // If the editing was started inside the name or value areas,
        // select accordingly.
        if (aEvent && aEvent.target === name) {
          aEditor.input.setSelectionRange(0, name.textContent.length);
        } else if (aEvent && aEvent.target === val) {
          let length = editValueDisplayed.length;
          let editorLength = aEditor.input.value.length;
          let start = editorLength - (length + 1);
          aEditor.input.setSelectionRange(start, start + length);
        } else {
          aEditor.input.select();
        }
      },
      done: (aVal, aCommit) => {
        if (!aCommit || aVal === initial) {
          return;
        }

        let doMods = this._startModifyingAttributes();
        let undoMods = this._startModifyingAttributes();

        // Remove the attribute stored in this editor and re-add any attributes
        // parsed out of the input element. Restore original attribute if
        // parsing fails.
        try {
          this._saveAttribute(aAttr.name, undoMods);
          doMods.removeAttribute(aAttr.name);
          this._applyAttributes(aVal, attr, doMods, undoMods);
          this.undo.do(() => {
            doMods.apply();
          }, () => {
            undoMods.apply();
          })
        } catch(ex) {
          console.error(ex);
        }
      }
    });

    // Figure out where we should place the attribute.
    let before = aBefore;
    if (aAttr.name == "id") {
      before = this.attrList.firstChild;
    } else if (aAttr.name == "class") {
      let idNode = this.attrs["id"];
      before = idNode ? idNode.nextSibling : this.attrList.firstChild;
    }
    this.attrList.insertBefore(attr, before);

    // Remove the old version of this attribute from the DOM.
    let oldAttr = this.attrs[aAttr.name];
    if (oldAttr && oldAttr.parentNode) {
      oldAttr.parentNode.removeChild(oldAttr);
    }

    this.attrs[aAttr.name] = attr;

    let collapsedValue;
    if (aAttr.value.match(COLLAPSE_DATA_URL_REGEX)) {
      collapsedValue = truncateString(aAttr.value, COLLAPSE_DATA_URL_LENGTH);
    } else {
      collapsedValue = truncateString(aAttr.value, COLLAPSE_ATTRIBUTE_LENGTH);
    }

    name.textContent = aAttr.name;
    val.textContent = collapsedValue;

    return attr;
  },

  /**
   * Parse a user-entered attribute string and apply the resulting
   * attributes to the node.  This operation is undoable.
   *
   * @param string aValue the user-entered value.
   * @param Element aAttrNode the attribute editor that created this
   *        set of attributes, used to place new attributes where the
   *        user put them.
   */
  _applyAttributes: function(aValue, aAttrNode, aDoMods, aUndoMods) {
    let attrs = parseAttributeValues(aValue, this.doc);
    for (let attr of attrs) {
      // Create an attribute editor next to the current attribute if needed.
      this._createAttribute(attr, aAttrNode ? aAttrNode.nextSibling : null);
      this._saveAttribute(attr.name, aUndoMods);
      aDoMods.setAttribute(attr.name, attr.value);
    }
  },

  /**
   * Saves the current state of the given attribute into an attribute
   * modification list.
   */
  _saveAttribute: function(aName, aUndoMods) {
    let node = this.node;
    if (node.hasAttribute(aName)) {
      let oldValue = node.getAttribute(aName);
      aUndoMods.setAttribute(aName, oldValue);
    } else {
      aUndoMods.removeAttribute(aName);
    }
  },

  /**
   * Called when the tag name editor has is done editing.
   */
  onTagEdit: function(aVal, aCommit) {
    if (!aCommit || aVal == this.rawNode.tagName) {
      return;
    }

    // Create a new element with the same attributes as the
    // current element and prepare to replace the current node
    // with it.
    try {
      var newElt = nodeDocument(this.rawNode).createElement(aVal);
    } catch(x) {
      // Failed to create a new element with that tag name, ignore
      // the change.
      return;
    }

    let attrs = this.rawNode.attributes;

    for (let i = 0 ; i < attrs.length; i++) {
      newElt.setAttribute(attrs[i].name, attrs[i].value);
    }
    let newFront = this.markup.walker.frontForRawNode(newElt);
    let newContainer = this.markup.importNode(newFront);

    // Retain the two nodes we care about here so we can undo.
    let walker = this.markup.walker;
    promise.all([
      walker.retainNode(newFront), walker.retainNode(this.node)
    ]).then(() => {
      function swapNodes(aOld, aNew) {
        aOld.parentNode.insertBefore(aNew, aOld);
        while (aOld.firstChild) {
          aNew.appendChild(aOld.firstChild);
        }
        aOld.parentNode.removeChild(aOld);
      }

      this.undo.do(() => {
        swapNodes(this.rawNode, newElt);
        this.markup.setNodeExpanded(newFront, this.container.expanded);
        if (this.container.selected) {
          this.markup.navigate(newContainer);
        }
      }, () => {
        swapNodes(newElt, this.rawNode);
        this.markup.setNodeExpanded(this.node, newContainer.expanded);
        if (newContainer.selected) {
          this.markup.navigate(this.container);
        }
      });
    }).then(null, console.error);
  },

  destroy: function() {}
};

function nodeDocument(node) {
  return node.ownerDocument ||
    (node.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE ? node : null);
}

function truncateString(str, maxLength) {
  if (str.length <= maxLength) {
    return str;
  }

  return str.substring(0, Math.ceil(maxLength / 2)) +
         "…" +
         str.substring(str.length - Math.floor(maxLength / 2));
}
/**
 * Parse attribute names and values from a string.
 *
 * @param  {String} attr
 *         The input string for which names/values are to be parsed.
 * @param  {HTMLDocument} doc
 *         A document that can be used to test valid attributes.
 * @return {Array}
 *         An array of attribute names and their values.
 */
function parseAttributeValues(attr, doc) {
  attr = attr.trim();

  // Handle bad user inputs by appending a " or ' if it fails to parse without them.
  let el = DOMParser.parseFromString("<div " + attr + "></div>", "text/html").body.childNodes[0] ||
           DOMParser.parseFromString("<div " + attr + "\"></div>", "text/html").body.childNodes[0] ||
           DOMParser.parseFromString("<div " + attr + "'></div>", "text/html").body.childNodes[0];
  let div = doc.createElement("div");

  let attributes = [];
  for (let attribute of el.attributes) {
    // Try to set on an element in the document, throws exception on bad input.
    // Prevents InvalidCharacterError - "String contains an invalid character".
    try {
      div.setAttribute(attribute.name, attribute.value);
      attributes.push({
        name: attribute.name,
        value: attribute.value
      });
    }
    catch(e) { }
  }

  // Attributes return from DOMParser in reverse order from how they are entered.
  return attributes.reverse();
}

loader.lazyGetter(MarkupView.prototype, "strings", () => Services.strings.createBundle(
  "chrome://browser/locale/devtools/inspector.properties"
));

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});
