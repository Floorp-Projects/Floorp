/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;

// Page size for pageup/pagedown
const PAGE_SIZE = 10;

const PREVIEW_AREA = 700;
const DEFAULT_MAX_CHILDREN = 100;

this.EXPORTED_SYMBOLS = ["MarkupView"];

Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");
Cu.import("resource:///modules/devtools/CssRuleView.jsm");
Cu.import("resource:///modules/devtools/Templater.jsm");
Cu.import("resource:///modules/devtools/Undo.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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
this.MarkupView = function MarkupView(aInspector, aFrame, aControllerWindow)
{
  this._inspector = aInspector;
  this._frame = aFrame;
  this.doc = this._frame.contentDocument;
  this._elt = this.doc.querySelector("#root");

  try {
    this.maxChildren = Services.prefs.getIntPref("devtools.markup.pagesize");
  } catch(ex) {
    this.maxChildren = DEFAULT_MAX_CHILDREN;
  }

  this.undo = new UndoStack();
  this.undo.installController(aControllerWindow);

  this._containers = new WeakMap();

  this._observer = new this.doc.defaultView.MutationObserver(this._mutationObserver.bind(this));

  this._boundOnNewSelection = this._onNewSelection.bind(this);
  this._inspector.selection.on("new-node", this._boundOnNewSelection);
  this._onNewSelection();

  this._boundKeyDown = this._onKeyDown.bind(this);
  this._frame.contentWindow.addEventListener("keydown", this._boundKeyDown, false);

  this._boundFocus = this._onFocus.bind(this);
  this._frame.addEventListener("focus", this._boundFocus, false);

  this._initPreview();
}

MarkupView.prototype = {
  _selectedContainer: null,

  template: function MT_template(aName, aDest, aOptions={stack: "markup-view.xhtml"})
  {
    let node = this.doc.getElementById("template-" + aName).cloneNode(true);
    node.removeAttribute("id");
    template(node, aDest, aOptions);
    return node;
  },

  /**
   * Get the MarkupContainer object for a given node, or undefined if
   * none exists.
   */
  getContainer: function MT_getContainer(aNode)
  {
    return this._containers.get(aNode);
  },

  /**
   * Highlight the inspector selected node.
   */
  _onNewSelection: function MT__onNewSelection()
  {
    if (this._inspector.selection.isNode()) {
      this.showNode(this._inspector.selection.node, true);
      this.markNodeAsSelected(this._inspector.selection.node);
    } else {
      this.unmarkSelectedNode();
    }
  },

  /**
   * Create a TreeWalker to find the next/previous
   * node for selection.
   */
  _selectionWalker: function MT__seletionWalker(aStart)
  {
    let walker = this.doc.createTreeWalker(
      aStart || this._elt,
      Ci.nsIDOMNodeFilter.SHOW_ELEMENT,
      function(aElement) {
        if (aElement.container && aElement.container.visible) {
          return Ci.nsIDOMNodeFilter.FILTER_ACCEPT;
        }
        return Ci.nsIDOMNodeFilter.FILTER_SKIP;
      },
      false
    );
    walker.currentNode = this._selectedContainer.elt;
    return walker;
  },

  /**
   * Key handling.
   */
  _onKeyDown: function MT__KeyDown(aEvent)
  {
    let handled = true;

    // Ignore keystrokes that originated in editors.
    if (aEvent.target.tagName.toLowerCase() === "input" ||
        aEvent.target.tagName.toLowerCase() === "textarea") {
      return;
    }

    switch(aEvent.keyCode) {
      case Ci.nsIDOMKeyEvent.DOM_VK_DELETE:
      case Ci.nsIDOMKeyEvent.DOM_VK_BACK_SPACE:
        this.deleteNode(this._selectedContainer.node);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_HOME:
        this.navigate(this._containers.get(this._rootNode.firstChild));
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_LEFT:
        this.collapseNode(this._selectedContainer.node);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_RIGHT:
        this.expandNode(this._selectedContainer.node);
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
  deleteNode: function MC__deleteNode(aNode)
  {
    let doc = nodeDocument(aNode);
    if (aNode === doc ||
        aNode === doc.documentElement ||
        aNode.nodeType == Ci.nsIDOMNode.DOCUMENT_TYPE_NODE) {
      return;
    }

    let parentNode = aNode.parentNode;
    let sibling = aNode.nextSibling;

    this.undo.do(function() {
      if (aNode.selected) {
        this.navigate(this._containers.get(parentNode));
      }
      parentNode.removeChild(aNode);
    }.bind(this), function() {
      parentNode.insertBefore(aNode, sibling);
    });
  },

  /**
   * If an editable item is focused, select its container.
   */
  _onFocus: function MC__onFocus(aEvent) {
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
  navigate: function MT__navigate(aContainer, aIgnoreFocus)
  {
    if (!aContainer) {
      return;
    }

    let node = aContainer.node;
    this.showNode(node, false);

    this._inspector.selection.setNode(node, "treepanel");
    // This event won't be fired if the node is the same. But the highlighter
    // need to lock the node if it wasn't.
    this._inspector.selection.emit("new-node");

    if (!aIgnoreFocus) {
      aContainer.focus();
    }
  },

  /**
   * Make sure a node is included in the markup tool.
   *
   * @param DOMNode aNode
   *        The node in the content document.
   *
   * @returns MarkupContainer The MarkupContainer object for this element.
   */
  importNode: function MT_importNode(aNode, aExpand)
  {
    if (!aNode) {
      return null;
    }

    if (this._containers.has(aNode)) {
      return this._containers.get(aNode);
    }

    this._observer.observe(aNode, {
      attributes: true,
      childList: true,
      characterData: true,
    });

    let walker = documentWalker(aNode);
    let parent = walker.parentNode();
    if (parent) {
      var container = new MarkupContainer(this, aNode);
    } else {
      var container = new RootContainer(this, aNode);
      this._elt.appendChild(container.elt);
      this._rootNode = aNode;
      aNode.addEventListener("load", function MP_watch_contentLoaded(aEvent) {
        // Fake a childList mutation here.
        this._mutationObserver([{target: aEvent.target, type: "childList"}]);
      }.bind(this), true);
    }

    this._containers.set(aNode, container);
    // FIXME: set an expando to prevent the the wrapper from disappearing
    // See bug 819131 for details.
    aNode.__preserveHack = true;
    container.expanded = aExpand;

    container.childrenDirty = true;
    this._updateChildren(container);

    if (parent) {
      this.importNode(parent, true);
    }
    return container;
  },

  /**
   * Mutation observer used for included nodes.
   */
  _mutationObserver: function MT__mutationObserver(aMutations)
  {
    for (let mutation of aMutations) {
      let container = this._containers.get(mutation.target);
      if (!container) {
        // Container might not exist if this came from a load event for an iframe
        // we're not viewing.
        continue;
      }
      if (mutation.type === "attributes" || mutation.type === "characterData") {
        container.update();
      } else if (mutation.type === "childList") {
        container.childrenDirty = true;
        this._updateChildren(container);
      }
    }
    this._inspector.emit("markupmutation");
  },

  /**
   * Make sure the given node's parents are expanded and the
   * node is scrolled on to screen.
   */
  showNode: function MT_showNode(aNode, centered)
  {
    let container = this.importNode(aNode);
    this._updateChildren(container);
    let walker = documentWalker(aNode);
    let parent;
    while (parent = walker.parentNode()) {
      this._updateChildren(this.getContainer(parent));
      this.expandNode(parent);
    }
    LayoutHelpers.scrollIntoViewIfNeeded(this._containers.get(aNode).editor.elt, centered);
  },

  /**
   * Expand the container's children.
   */
  _expandContainer: function MT__expandContainer(aContainer)
  {
    if (aContainer.hasChildren && !aContainer.expanded) {
      aContainer.expanded = true;
      this._updateChildren(aContainer);
    }
  },

  /**
   * Expand the node's children.
   */
  expandNode: function MT_expandNode(aNode)
  {
    let container = this._containers.get(aNode);
    this._expandContainer(container);
  },

  /**
   * Expand the entire tree beneath a container.
   *
   * @param aContainer The container to expand.
   */
  _expandAll: function MT_expandAll(aContainer)
  {
    this._expandContainer(aContainer);
    let child = aContainer.children.firstChild;
    while (child) {
      this._expandAll(child.container);
      child = child.nextSibling;
    }
  },

  /**
   * Expand the entire tree beneath a node.
   *
   * @param aContainer The node to expand, or null
   *        to start from the top.
   */
  expandAll: function MT_expandAll(aNode)
  {
    aNode = aNode || this._rootNode;
    this._expandAll(this._containers.get(aNode));
  },

  /**
   * Collapse the node's children.
   */
  collapseNode: function MT_collapseNode(aNode)
  {
    let container = this._containers.get(aNode);
    container.expanded = false;
  },

  /**
   * Mark the given node selected.
   */
  markNodeAsSelected: function MT_markNodeAsSelected(aNode)
  {
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

    this._ensureSelectionVisible();
    this._selectedContainer.focus();

    return true;
  },

  /**
   * Make sure that every ancestor of the selection are updated
   * and included in the list of visible children.
   */
  _ensureSelectionVisible: function MT_ensureSelectionVisible()
  {
    let node = this._selectedContainer.node;
    let walker = documentWalker(node);
    while (node) {
      let container = this._containers.get(node);
      let parent = walker.parentNode();
      if (!container.elt.parentNode) {
        let parentContainer = this._containers.get(parent);
        parentContainer.childrenDirty = true;
        this._updateChildren(parentContainer, node);
      }

      node = parent;
    }
  },

  /**
   * Unmark selected node (no node selected).
   */
  unmarkSelectedNode: function MT_unmarkSelectedNode()
  {
    if (this._selectedContainer) {
      this._selectedContainer.selected = false;
      this._selectedContainer = null;
    }
  },

  /**
   * Called when the markup panel initiates a change on a node.
   */
  nodeChanged: function MT_nodeChanged(aNode)
  {
    if (aNode === this._inspector.selection) {
      this._inspector.change("markupview");
    }
  },

  /**
   * Make sure all children of the given container's node are
   * imported and attached to the container in the right order.
   * @param aCentered If provided, this child will be included
   *        in the visible subset, and will be roughly centered
   *        in that list.
   */
  _updateChildren: function MT__updateChildren(aContainer, aCentered)
  {
    if (!aContainer.childrenDirty) {
      return false;
    }

    // Get a tree walker pointing at the first child of the node.
    let treeWalker = documentWalker(aContainer.node);
    let child = treeWalker.firstChild();
    aContainer.hasChildren = !!child;

    if (!aContainer.expanded) {
      return;
    }

    aContainer.childrenDirty = false;

    let children = this._getVisibleChildren(aContainer, aCentered);
    let fragment = this.doc.createDocumentFragment();

    for (child of children.children) {
      let container = this.importNode(child, false);
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
                  [aContainer.node.children.length.toString()], 1),
        allButtonClick: function() {
          aContainer.maxChildren = -1;
          aContainer.childrenDirty = true;
          this._updateChildren(aContainer);
        }.bind(this)
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

    return true;
  },

  /**
   * Return a list of the children to display for this container.
   */
  _getVisibleChildren: function MV__getVisibleChildren(aContainer, aCentered)
  {
    let maxChildren = aContainer.maxChildren || this.maxChildren;
    if (maxChildren == -1) {
      maxChildren = Number.MAX_VALUE;
    }
    let firstChild = documentWalker(aContainer.node).firstChild();
    let lastChild = documentWalker(aContainer.node).lastChild();

    if (!firstChild) {
      // No children, we're done.
      return { hasFirst: true, hasLast: true, children: [] };
    }

    // By default try to put the selected child in the middle of the list.
    let start = aCentered || firstChild;

    // Start by reading backward from the starting point....
    let nodes = [];
    let backwardWalker = documentWalker(start);
    if (backwardWalker.previousSibling()) {
      let backwardCount = Math.floor(maxChildren / 2);
      let backwardNodes = this._readBackward(backwardWalker, backwardCount);
      nodes = backwardNodes;
    }

    // Then read forward by any slack left in the max children...
    let forwardWalker = documentWalker(start);
    let forwardCount = maxChildren - nodes.length;
    nodes = nodes.concat(this._readForward(forwardWalker, forwardCount));

    // If there's any room left, it means we've run all the way to the end.
    // In that case, there might still be more items at the front.
    let remaining = maxChildren - nodes.length;
    if (remaining > 0 && nodes[0] != firstChild) {
      let firstNodes = this._readBackward(backwardWalker, remaining);

      // Then put it all back together.
      nodes = firstNodes.concat(nodes);
    }

    return {
      hasFirst: nodes[0] == firstChild,
      hasLast: nodes[nodes.length - 1] == lastChild,
      children: nodes
    };
  },

  _readForward: function MV__readForward(aWalker, aCount)
  {
    let ret = [];
    let node = aWalker.currentNode;
    do {
      ret.push(node);
      node = aWalker.nextSibling();
    } while (node && --aCount);
    return ret;
  },

  _readBackward: function MV__readBackward(aWalker, aCount)
  {
    let ret = [];
    let node = aWalker.currentNode;
    do {
      ret.push(node);
      node = aWalker.previousSibling();
    } while(node && --aCount);
    ret.reverse();
    return ret;
  },

  /**
   * Tear down the markup panel.
   */
  destroy: function MT_destroy()
  {
    this.undo.destroy();
    delete this.undo;

    this._frame.removeEventListener("focus", this._boundFocus, false);
    delete this._boundFocus;

    this._frame.contentWindow.removeEventListener("scroll", this._boundUpdatePreview, true);
    this._frame.contentWindow.removeEventListener("resize", this._boundUpdatePreview, true);
    this._frame.contentWindow.removeEventListener("overflow", this._boundResizePreview, true);
    this._frame.contentWindow.removeEventListener("underflow", this._boundResizePreview, true);
    delete this._boundUpdatePreview;

    this._frame.contentWindow.removeEventListener("keydown", this._boundKeyDown, true);
    delete this._boundKeyDown;

    this._inspector.selection.off("new-node", this._boundOnNewSelection);
    delete this._boundOnNewSelection;

    delete this._elt;

    delete this._containers;
    this._observer.disconnect();
    delete this._observer;
  },

  /**
   * Initialize the preview panel.
   */
  _initPreview: function MT_initPreview()
  {
    if (!Services.prefs.getBoolPref("devtools.inspector.markupPreview")) {
      return;
    }

    this._previewBar = this.doc.querySelector("#previewbar");
    this._preview = this.doc.querySelector("#preview");
    this._viewbox = this.doc.querySelector("#viewbox");

    this._previewBar.classList.remove("disabled");

    this._previewWidth = this._preview.getBoundingClientRect().width;

    this._boundResizePreview = this._resizePreview.bind(this);
    this._frame.contentWindow.addEventListener("resize", this._boundResizePreview, true);
    this._frame.contentWindow.addEventListener("overflow", this._boundResizePreview, true);
    this._frame.contentWindow.addEventListener("underflow", this._boundResizePreview, true);

    this._boundUpdatePreview = this._updatePreview.bind(this);
    this._frame.contentWindow.addEventListener("scroll", this._boundUpdatePreview, true);
    this._updatePreview();
  },


  /**
   * Move the preview viewbox.
   */
  _updatePreview: function MT_updatePreview()
  {
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
      this._previewBar.setAttribute("style", "height:" + height + "px;transform:translateY(" + scrollTo + "px)");
    } else {
      this._previewBar.setAttribute("style", "height:100%");
    }

    let bgSize = ~~width + "px " + ~~height + "px";
    this._preview.setAttribute("style", "background-size:" + bgSize);

    let height = ~~(win.innerHeight * ratio) + "px";
    let top = ~~(win.scrollY * ratio) + "px";
    this._viewbox.setAttribute("style", "height:" + height + ";transform: translateY(" + top + ")");
  },

  /**
   * Hide the preview while resizing, to avoid slowness.
   */
  _resizePreview: function MT_resizePreview()
  {
    let win = this._frame.contentWindow;
    this._previewBar.classList.add("hide");
    win.clearTimeout(this._resizePreviewTimeout);

    win.setTimeout(function() {
      this._updatePreview();
      this._previewBar.classList.remove("hide");
    }.bind(this), 1000);
  },

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
 */
function MarkupContainer(aMarkupView, aNode)
{
  this.markup = aMarkupView;
  this.doc = this.markup.doc;
  this.undo = this.markup.undo;
  this.node = aNode;

  if (aNode.nodeType == Ci.nsIDOMNode.TEXT_NODE) {
    this.editor = new TextEditor(this, aNode, "text");
  } else if (aNode.nodeType == Ci.nsIDOMNode.COMMENT_NODE) {
    this.editor = new TextEditor(this, aNode, "comment");
  } else if (aNode.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
    this.editor = new ElementEditor(this, aNode);
  } else if (aNode.nodeType == Ci.nsIDOMNode.DOCUMENT_TYPE_NODE) {
    this.editor = new DoctypeEditor(this, aNode);
  } else {
    this.editor = new GenericEditor(this.markup, aNode);
  }

  // The template will fill the following properties
  this.elt = null;
  this.expander = null;
  this.codeBox = null;
  this.children = null;
  this.markup.template("container", this);
  this.elt.container = this;

  this.expander.addEventListener("click", function() {
    this.markup.navigate(this);

    if (this.expanded) {
      this.markup.collapseNode(this.node);
    } else {
      this.markup.expandNode(this.node);
    }
  }.bind(this));

  this.codeBox.insertBefore(this.editor.elt, this.children);

  this.editor.elt.addEventListener("mousedown", function(evt) {
    this.markup.navigate(this);
  }.bind(this), false);

  if (this.editor.closeElt) {
    this.codeBox.appendChild(this.editor.closeElt);
  }

}

MarkupContainer.prototype = {
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

  /**
   * True if the node has been visually expanded in the tree.
   */
  get expanded() {
    return this.children.hasAttribute("expanded");
  },

  set expanded(aValue) {
    if (aValue) {
      this.expander.setAttribute("expanded", "");
      this.children.setAttribute("expanded", "");
    } else {
      this.expander.removeAttribute("expanded");
      this.children.removeAttribute("expanded");
    }
  },

  /**
   * True if the container is visible in the markup tree.
   */
  get visible()
  {
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
    this._selected = aValue;
    if (this._selected) {
      this.editor.elt.classList.add("selected");
      if (this.editor.closeElt) {
        this.editor.closeElt.classList.add("selected");
      }
    } else {
      this.editor.elt.classList.remove("selected");
      if (this.editor.closeElt) {
        this.editor.closeElt.classList.remove("selected");
      }
    }
  },

  /**
   * Update the container's editor to the current state of the
   * viewed node.
   */
  update: function MC_update()
  {
    if (this.editor.update) {
      this.editor.update();
    }
  },

  /**
   * Try to put keyboard focus on the current editor.
   */
  focus: function MC_focus()
  {
    let focusable = this.editor.elt.querySelector("[tabindex]");
    if (focusable) {
      focusable.focus();
    }
  },
}

/**
 * Dummy container node used for the root document element.
 */
function RootContainer(aMarkupView, aNode)
{
  this.doc = aMarkupView.doc;
  this.elt = this.doc.createElement("ul");
  this.children = this.elt;
  this.node = aNode;
}

/**
 * Creates an editor for simple nodes.
 */
function GenericEditor(aContainer, aNode)
{
  this.elt = aContainer.doc.createElement("span");
  this.elt.className = "editor";
  this.elt.textContent = aNode.nodeName;
}

/**
 * Creates an editor for a DOCTYPE node.
 *
 * @param MarkupContainer aContainer The container owning this editor.
 * @param DOMNode aNode The node being edited.
 */
function DoctypeEditor(aContainer, aNode)
{
  this.elt = aContainer.doc.createElement("span");
  this.elt.className = "editor comment";
  this.elt.textContent = '<!DOCTYPE ' + aNode.name +
     (aNode.publicId ? ' PUBLIC "' +  aNode.publicId + '"': '') +
     (aNode.systemId ? ' "' + aNode.systemId + '"' : '') +
     '>';
}

/**
 * Creates a simple text editor node, used for TEXT and COMMENT
 * nodes.
 *
 * @param MarkupContainer aContainer The container owning this editor.
 * @param DOMNode aNode The node being edited.
 * @param string aTemplate The template id to use to build the editor.
 */
function TextEditor(aContainer, aNode, aTemplate)
{
  this.node = aNode;

  aContainer.markup.template(aTemplate, this);

  _editableField({
    element: this.value,
    stopOnReturn: true,
    trigger: "dblclick",
    multiline: true,
    done: function TE_done(aVal, aCommit) {
      if (!aCommit) {
        return;
      }
      let oldValue = this.node.nodeValue;
      aContainer.undo.do(function() {
        this.node.nodeValue = aVal;
        aContainer.markup.nodeChanged(this.node);
      }.bind(this), function() {
        this.node.nodeValue = oldValue;
        aContainer.markup.nodeChanged(this.node);
      }.bind(this));
    }.bind(this)
  });

  this.update();
}

TextEditor.prototype = {
  update: function TE_update()
  {
    this.value.textContent = this.node.nodeValue;
  }
};

/**
 * Creates an editor for an Element node.
 *
 * @param MarkupContainer aContainer The container owning this editor.
 * @param Element aNode The node being edited.
 */
function ElementEditor(aContainer, aNode)
{
  this.doc = aContainer.doc;
  this.undo = aContainer.undo;
  this.template = aContainer.markup.template.bind(aContainer.markup);
  this.container = aContainer;
  this.markup = this.container.markup;
  this.node = aNode;

  this.attrs = [];

  // The templates will fill the following properties
  this.elt = null;
  this.tag = null;
  this.attrList = null;
  this.newAttr = null;
  this.closeElt = null;

  // Create the main editor
  this.template("element", this);

  // Create the closing tag
  this.template("elementClose", this);

  // Make the tag name editable (unless this is a document element)
  if (aNode != aNode.ownerDocument.documentElement) {
    this.tag.setAttribute("tabindex", "0");
    _editableField({
      element: this.tag,
      trigger: "dblclick",
      stopOnReturn: true,
      done: this.onTagEdit.bind(this),
    });
  }

  // Make the new attribute space editable.
  _editableField({
    element: this.newAttr,
    trigger: "dblclick",
    stopOnReturn: true,
    done: function EE_onNew(aVal, aCommit) {
      if (!aCommit) {
        return;
      }

      try {
        this._applyAttributes(aVal);
      } catch (x) {
        return;
      }
    }.bind(this)
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
  update: function EE_update()
  {
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
    for (let i = 0; i < attrs.length; i++) {
      let attr = this._createAttribute(attrs[i]);
      if (!attr.inplaceEditor) {
        attr.style.removeProperty("display");
      }
    }
  },

  _createAttribute: function EE_createAttribute(aAttr, aBefore)
  {
    if (aAttr.name in this.attrs) {
      var attr = this.attrs[aAttr.name];
      var name = attr.querySelector(".attrname");
      var val = attr.querySelector(".attrvalue");
    } else {
      // Create the template editor, which will save some variables here.
      let data = {
        attrName: aAttr.name,
      };
      this.template("attribute", data);
      var {attr, inner, name, val} = data;

      // Figure out where we should place the attribute.
      let before = aBefore || null;
      if (aAttr.name == "id") {
        before = this.attrList.firstChild;
      } else if (aAttr.name == "class") {
        let idNode = this.attrs["id"];
        before = idNode ? idNode.nextSibling : this.attrList.firstChild;
      }
      this.attrList.insertBefore(attr, before);

      // Make the attribute editable.
      _editableField({
        element: inner,
        trigger: "dblclick",
        stopOnReturn: true,
        selectAll: false,
        start: function EE_editAttribute_start(aEditor, aEvent) {
          // If the editing was started inside the name or value areas,
          // select accordingly.
          if (aEvent && aEvent.target === name) {
            aEditor.input.setSelectionRange(0, name.textContent.length);
          } else if (aEvent && aEvent.target === val) {
            let length = val.textContent.length;
            let editorLength = aEditor.input.value.length;
            let start = editorLength - (length + 1);
            aEditor.input.setSelectionRange(start, start + length);
          } else {
            aEditor.input.select();
          }
        },
        done: function EE_editAttribute_done(aVal, aCommit) {
          if (!aCommit) {
            return;
          }

          this.undo.startBatch();

          // Remove the attribute stored in this editor and re-add any attributes
          // parsed out of the input element. Restore original attribute if
          // parsing fails.
          this._removeAttribute(this.node, aAttr.name);
          try {
            this._applyAttributes(aVal, attr);
            this.undo.endBatch();
          } catch (e) {
            this.undo.endBatch();
            this.undo.undo();
          }
        }.bind(this)
      });

      this.attrs[aAttr.name] = attr;
    }

    name.textContent = aAttr.name;
    val.textContent = aAttr.value;

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
   * @throws SYNTAX_ERR if aValue is not well-formed.
   */
  _applyAttributes: function EE__applyAttributes(aValue, aAttrNode)
  {
    // Create a dummy node for parsing the attribute list.
    let dummyNode = this.doc.createElement("div");

    let parseTag = (this.node.namespaceURI.match(/svg/i) ? "svg" :
                   (this.node.namespaceURI.match(/mathml/i) ? "math" : "div"));
    let parseText = "<" + parseTag + " " + aValue + "/>";
    // Throws exception if parseText is not well-formed.
    dummyNode.innerHTML = parseText;
    let parsedNode = dummyNode.firstChild;

    let attrs = parsedNode.attributes;

    this.undo.startBatch();

    for (let i = 0; i < attrs.length; i++) {
      // Create an attribute editor next to the current attribute if needed.
      this._createAttribute(attrs[i], aAttrNode ? aAttrNode.nextSibling : null);
      this._setAttribute(this.node, attrs[i].name, attrs[i].value);
    }

    this.undo.endBatch();
  },

  /**
   * Helper function for _setAttribute and _removeAttribute,
   * returns a function that puts an attribute back the way it was.
   */
  _restoreAttribute: function EE_restoreAttribute(aNode, aName)
  {
    if (aNode.hasAttribute(aName)) {
      let oldValue = aNode.getAttribute(aName);
      return function() {
        aNode.setAttribute(aName, oldValue);
        this.markup.nodeChanged(aNode);
      }.bind(this);
    } else {
      return function() {
        aNode.removeAttribute(aName);
        this.markup.nodeChanged(aNode);
      }.bind(this);
    }
  },

  /**
   * Sets an attribute.  This operation is undoable.
   */
  _setAttribute: function EE_setAttribute(aNode, aName, aValue)
  {
    this.undo.do(function() {
      aNode.setAttribute(aName, aValue);
      this.markup.nodeChanged(aNode);
    }.bind(this), this._restoreAttribute(aNode, aName));
  },

  /**
   * Removes an attribute.  This operation is undoable.
   */
  _removeAttribute: function EE_removeAttribute(aNode, aName)
  {
    this.undo.do(function() {
      aNode.removeAttribute(aName);
      this.markup.nodeChanged(aNode);
    }.bind(this), this._restoreAttribute(aNode, aName));
  },

  /**
   * Handler for the new attribute editor.
   */
  _onNewAttribute: function EE_onNewAttribute(aValue, aCommit)
  {
    if (!aValue || !aCommit) {
      return;
    }

    this._setAttribute(this.node, aValue, "");
    let attr = this._createAttribute({ name: aValue, value: ""});
    attr.style.removeAttribute("display");
    attr.querySelector("attrvalue").click();
  },


  /**
   * Called when the tag name editor has is done editing.
   */
  onTagEdit: function EE_onTagEdit(aVal, aCommit) {
    if (!aCommit || aVal == this.node.tagName) {
      return;
    }

    // Create a new element with the same attributes as the
    // current element and prepare to replace the current node
    // with it.
    try {
      var newElt = nodeDocument(this.node).createElement(aVal);
    } catch(x) {
      // Failed to create a new element with that tag name, ignore
      // the change.
      return;
    }

    let attrs = this.node.attributes;

    for (let i = 0 ; i < attrs.length; i++) {
      newElt.setAttribute(attrs[i].name, attrs[i].value);
    }

    function swapNodes(aOld, aNew) {
      while (aOld.firstChild) {
        aNew.appendChild(aOld.firstChild);
      }
      aOld.parentNode.insertBefore(aNew, aOld);
      aOld.parentNode.removeChild(aOld);
    }

    let markup = this.container.markup;

    // Queue an action to swap out the element.
    this.undo.do(function() {
      swapNodes(this.node, newElt);

      // Make sure the new node is imported and is expanded/selected
      // the same as the current node.
      let newContainer = markup.importNode(newElt, this.container.expanded);
      newContainer.expanded = this.container.expanded;
      if (this.container.selected) {
        markup.navigate(newContainer);
      }
    }.bind(this), function() {
      swapNodes(newElt, this.node);

      let newContainer = markup._containers.get(newElt);
      this.container.expanded = newContainer.expanded;
      if (newContainer.selected) {
        markup.navigate(this.container);
      }
    }.bind(this));
  },
}



RootContainer.prototype = {
  hasChildren: true,
  expanded: true,
  update: function RC_update() {}
};

function documentWalker(node) {
  return new DocumentWalker(node, Ci.nsIDOMNodeFilter.SHOW_ALL, whitespaceTextFilter, false);
}

function nodeDocument(node) {
  return node.ownerDocument || (node.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE ? node : null);
}

/**
 * Similar to a TreeWalker, except will dig in to iframes and it doesn't
 * implement the good methods like previousNode and nextNode.
 *
 * See TreeWalker documentation for explanations of the methods.
 */
function DocumentWalker(aNode, aShow, aFilter, aExpandEntityReferences)
{
  let doc = nodeDocument(aNode);
  this.walker = doc.createTreeWalker(nodeDocument(aNode),
    aShow, aFilter, aExpandEntityReferences);
  this.walker.currentNode = aNode;
  this.filter = aFilter;
}

DocumentWalker.prototype = {
  get node() this.walker.node,
  get whatToShow() this.walker.whatToShow,
  get expandEntityReferences() this.walker.expandEntityReferences,
  get currentNode() this.walker.currentNode,
  set currentNode(aVal) this.walker.currentNode = aVal,

  /**
   * Called when the new node is in a different document than
   * the current node, creates a new treewalker for the document we've
   * run in to.
   */
  _reparentWalker: function DW_reparentWalker(aNewNode) {
    if (!aNewNode) {
      return null;
    }
    let doc = nodeDocument(aNewNode);
    let walker = doc.createTreeWalker(doc,
      this.whatToShow, this.filter, this.expandEntityReferences);
    walker.currentNode = aNewNode;
    this.walker = walker;
    return aNewNode;
  },

  parentNode: function DW_parentNode()
  {
    let currentNode = this.walker.currentNode;
    let parentNode = this.walker.parentNode();

    if (!parentNode) {
      if (currentNode && currentNode.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE
          && currentNode.defaultView) {
        let embeddingFrame = currentNode.defaultView.frameElement;
        if (embeddingFrame) {
          return this._reparentWalker(embeddingFrame);
        }
      }
      return null;
    }

    return parentNode;
  },

  firstChild: function DW_firstChild()
  {
    let node = this.walker.currentNode;
    if (!node)
      return;
    if (node.contentDocument) {
      return this._reparentWalker(node.contentDocument);
    } else if (node instanceof nodeDocument(node).defaultView.GetSVGDocument) {
      return this._reparentWalker(node.getSVGDocument());
    }
    return this.walker.firstChild();
  },

  lastChild: function DW_lastChild()
  {
    let node = this.walker.currentNode;
    if (!node)
      return;
    if (node.contentDocument) {
      return this._reparentWalker(node.contentDocument);
    } else if (node instanceof nodeDocument(node).defaultView.GetSVGDocument) {
      return this._reparentWalker(node.getSVGDocument());
    }
    return this.walker.lastChild();
  },

  previousSibling: function DW_previousSibling() this.walker.previousSibling(),
  nextSibling: function DW_nextSibling() this.walker.nextSibling(),

  // XXX bug 785143: not doing previousNode or nextNode, which would sure be useful.
}

/**
 * A tree walker filter for avoiding empty whitespace text nodes.
 */
function whitespaceTextFilter(aNode)
{
    if (aNode.nodeType == Ci.nsIDOMNode.TEXT_NODE &&
        !/[^\s]/.exec(aNode.nodeValue)) {
      return Ci.nsIDOMNodeFilter.FILTER_SKIP;
    } else {
      return Ci.nsIDOMNodeFilter.FILTER_ACCEPT;
    }
}

XPCOMUtils.defineLazyGetter(MarkupView.prototype, "strings", function () {
  return Services.strings.createBundle(
          "chrome://browser/locale/devtools/inspector.properties");
});
