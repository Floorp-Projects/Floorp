/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
const NEW_SELECTION_HIGHLIGHTER_TIMER = 1000;
const GRAB_DELAY = 400;
const DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE = 50;
const DRAG_DROP_MIN_AUTOSCROLL_SPEED = 5;
const DRAG_DROP_MAX_AUTOSCROLL_SPEED = 15;
const AUTOCOMPLETE_POPUP_PANEL_ID = "markupview_autoCompletePopup";

const {UndoStack} = require("devtools/shared/undo");
const {editableField, InplaceEditor} = require("devtools/shared/inplace-editor");
const {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
const {HTMLEditor} = require("devtools/markupview/html-editor");
const promise = require("promise");
const {Tooltip} = require("devtools/shared/widgets/Tooltip");
const EventEmitter = require("devtools/toolkit/event-emitter");
const Heritage = require("sdk/core/heritage");
const {setTimeout, clearTimeout, setInterval, clearInterval} = require("sdk/timers");
const {parseAttribute} = require("devtools/shared/node-attribute-parser");
const ELLIPSIS = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data;
const {Task} = require("resource://gre/modules/Task.jsm");
const LayoutHelpers = require("devtools/toolkit/layout-helpers");

Cu.import("resource://gre/modules/devtools/Templater.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

loader.lazyGetter(this, "DOMParser", function() {
 return Cc["@mozilla.org/xmlextras/domparser;1"].createInstance(Ci.nsIDOMParser);
});
loader.lazyGetter(this, "AutocompletePopup", () => {
  return require("devtools/shared/autocomplete-popup").AutocompletePopup;
});

/**
 * Vocabulary for the purposes of this file:
 *
 * MarkupContainer - the structure that holds an editor and its
 *  immediate children in the markup panel.
 *  - MarkupElementContainer: markup container for element nodes
 *  - MarkupTextContainer: markup container for text / comment nodes
 *  - MarkupReadonlyContainer: markup container for other nodes
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
  this.win = this._frame.contentWindow;
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
    theme: "auto",
    // panelId option prevents the markupView autocomplete popup from
    // sharing XUL elements with other views, such as ruleView (see Bug 1191093)
    panelId: AUTOCOMPLETE_POPUP_PANEL_ID
  };
  this.popup = new AutocompletePopup(this.doc.defaultView.parent.document, options);

  this.undo = new UndoStack();
  this.undo.installController(aControllerWindow);

  this._containers = new Map();

  this._boundMutationObserver = this._mutationObserver.bind(this);
  this.walker.on("mutations", this._boundMutationObserver);

  this._boundOnDisplayChange = this._onDisplayChange.bind(this);
  this.walker.on("display-change", this._boundOnDisplayChange);

  this._onMouseClick = this._onMouseClick.bind(this);

  this._onMouseUp = this._onMouseUp.bind(this);
  this.doc.body.addEventListener("mouseup", this._onMouseUp);

  this._boundOnNewSelection = this._onNewSelection.bind(this);
  this._inspector.selection.on("new-node-front", this._boundOnNewSelection);
  this._onNewSelection();

  this._boundKeyDown = this._onKeyDown.bind(this);
  this._frame.contentWindow.addEventListener("keydown", this._boundKeyDown, false);

  this._onCopy = this._onCopy.bind(this);
  this._frame.contentWindow.addEventListener("copy", this._onCopy);

  this._boundFocus = this._onFocus.bind(this);
  this._frame.addEventListener("focus", this._boundFocus, false);

  this._makeTooltipPersistent = this._makeTooltipPersistent.bind(this);

  this._initPreview();
  this._initTooltips();
  this._initHighlighter();

  EventEmitter.decorate(this);
}

exports.MarkupView = MarkupView;

MarkupView.prototype = {
  /**
   * How long does a node flash when it mutates (in ms).
   */
  CONTAINER_FLASHING_DURATION: 500,

  _selectedContainer: null,

  _initTooltips: function() {
    this.tooltip = new Tooltip(this._inspector.panelDoc);
    this._makeTooltipPersistent(false);

    this._elt.addEventListener("click", this._onMouseClick, false);
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
      this.showNode(nodeFront).then(() => {
        this._showContainerAsHovered(nodeFront);
      });
    };
    this._inspector.toolbox.on("picker-node-hovered", this._onToolboxPickerHover);
  },

  _makeTooltipPersistent: function(state) {
    if (state) {
      this.tooltip.stopTogglingOnHover();
    } else {
      this.tooltip.startTogglingOnHover(this._elt,
        this._isImagePreviewTarget.bind(this));
    }
  },

  isDragging: false,

  _onMouseMove: function(event) {
    if (this.isDragging) {
      event.preventDefault();
      this._dragStartEl = event.target;

      let docEl = this.doc.documentElement;

      if (this._scrollInterval) {
        clearInterval(this._scrollInterval);
      }

      // Auto-scroll when the mouse approaches top/bottom edge
      let distanceFromBottom = docEl.clientHeight - event.pageY + this.win.scrollY,
          distanceFromTop = event.pageY - this.win.scrollY;

      if (distanceFromBottom <= DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE) {
        // Map our distance from 0-50 to 5-15 range so the speed is kept
        // in a range not too fast, not too slow
        let speed = map(distanceFromBottom, 0, DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE,
                        DRAG_DROP_MIN_AUTOSCROLL_SPEED, DRAG_DROP_MAX_AUTOSCROLL_SPEED);
        // Here, we use minus because the value of speed - 15 is always negative
        // and it makes the speed relative to the distance between mouse and edge
        // the closer to the edge, the faster
        this._scrollInterval = setInterval(() => {
          docEl.scrollTop -= speed - DRAG_DROP_MAX_AUTOSCROLL_SPEED;
        }, 0);
      }

      if (distanceFromTop <= DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE) {
        // refer to bottom edge's comments for more info
        let speed = map(distanceFromTop, 0, DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE,
                        DRAG_DROP_MIN_AUTOSCROLL_SPEED, DRAG_DROP_MAX_AUTOSCROLL_SPEED);

        this._scrollInterval = setInterval(() => {
          docEl.scrollTop += speed - DRAG_DROP_MAX_AUTOSCROLL_SPEED;
        }, 0);
      }

      return;
    };

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

  _onMouseClick: function(event) {
    // From the target passed here, let's find the parent MarkupContainer
    // and ask it if the tooltip should be shown
    let parentNode = event.target;
    let container;
    while (parentNode !== this.doc.body) {
      if (parentNode.container) {
        container = parentNode.container;
        break;
      }
      parentNode = parentNode.parentNode;
    }

    if (container instanceof MarkupElementContainer) {
      // With the newly found container, delegate the tooltip content creation
      // and decision to show or not the tooltip
      container._buildEventTooltipContent(event.target, this.tooltip);
    }
  },

  _onMouseUp: function() {
    if (this._lastDropTarget) {
      this.indicateDropTarget(null);
    }
    if (this._lastDragTarget) {
      this.indicateDragTarget(null);
    }
    if (this._scrollInterval) {
      clearInterval(this._scrollInterval);
    }
  },

  _hoveredNode: null,

  /**
   * Show a NodeFront's container as being hovered
   * @param {NodeFront} nodeFront The node to show as hovered
   */
  _showContainerAsHovered: function(nodeFront) {
    if (this._hoveredNode === nodeFront) {
      return;
    }

    if (this._hoveredNode) {
      this.getContainer(this._hoveredNode).hovered = false;
    }

    this.getContainer(nodeFront).hovered = true;
    this._hoveredNode = nodeFront;
  },

  _onMouseLeave: function() {
    if (this._scrollInterval) {
      clearInterval(this._scrollInterval);
    }
    if (this.isDragging) return;

    this._hideBoxModel(true);
    if (this._hoveredNode) {
      this.getContainer(this._hoveredNode).hovered = false;
    }
    this._hoveredNode = null;
  },

  /**
   * Show the box model highlighter on a given node front
   * @param {NodeFront} nodeFront The node to show the highlighter for
   * @return a promise that resolves when the highlighter for this nodeFront is
   * shown, taking into account that there could already be highlighter requests
   * queued up
   */
  _showBoxModel: function(nodeFront) {
    return this._inspector.toolbox.highlighterUtils.highlightNodeFront(nodeFront);
  },

  /**
   * Hide the box model highlighter on a given node front
   * @param {NodeFront} nodeFront The node to hide the highlighter for
   * @param {Boolean} forceHide See toolbox-highlighter-utils/unhighlight
   * @return a promise that resolves when the highlighter for this nodeFront is
   * hidden, taking into account that there could already be highlighter requests
   * queued up
   */
  _hideBoxModel: function(forceHide) {
    return this._inspector.toolbox.highlighterUtils.unhighlight(forceHide);
  },

  _briefBoxModelTimer: null,

  _clearBriefBoxModelTimer: function() {
    if (this._briefBoxModelTimer) {
      clearTimeout(this._briefBoxModelTimer);
      this._briefBoxModelPromise.resolve();
      this._briefBoxModelPromise = null;
      this._briefBoxModelTimer = null;
    }
  },

  _brieflyShowBoxModel: function(nodeFront) {
    this._clearBriefBoxModelTimer();
    let onShown = this._showBoxModel(nodeFront);
    this._briefBoxModelPromise = promise.defer();

    this._briefBoxModelTimer = setTimeout(() => {
      this._hideBoxModel()
          .then(this._briefBoxModelPromise.resolve,
                this._briefBoxModelPromise.resolve);
    }, NEW_SELECTION_HIGHLIGHTER_TIMER);

    return promise.all([onShown, this._briefBoxModelPromise.promise]);
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
    let updateChildren = (node) => {
      this.getContainer(node).update();
      for (let child of node.treeChildren()) {
        updateChildren(child);
      }
    };

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

  /**
   * Executed when the mouse hovers over a target in the markup-view and is used
   * to decide whether this target should be used to display an image preview
   * tooltip.
   * Delegates the actual decision to the corresponding MarkupContainer instance
   * if one is found.
   * @return the promise returned by MarkupElementContainer._isImagePreviewTarget
   */
  _isImagePreviewTarget: function(target) {
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

    if (container instanceof MarkupElementContainer) {
      // With the newly found container, delegate the tooltip content creation
      // and decision to show or not the tooltip
      return container.isImagePreviewTarget(target, this.tooltip);
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
    let unwantedReasons = ["inspector-open", "navigateaway", "nodeselected", "test"];
    let isHighlitNode = this._hoveredNode === this._inspector.selection.nodeFront;
    return !isHighlitNode && reason && unwantedReasons.indexOf(reason) === -1;
  },

  /**
   * React to new-node-front selection events.
   * Highlights the node if needed, and make sure it is shown and selected in
   * the view.
   */
  _onNewSelection: function() {
    let selection = this._inspector.selection;
    let reason = selection.reason;

    this.htmlEditor.hide();
    if (this._hoveredNode && this._hoveredNode !== selection.nodeFront) {
      this.getContainer(this._hoveredNode).hovered = false;
      this._hoveredNode = null;
    }

    if (!selection.isNode()) {
      this.unmarkSelectedNode();
      return;
    }

    let done = this._inspector.updating("markup-view");
    let onShowBoxModel, onShow;

    // Highlight the element briefly if needed.
    if (this._shouldNewSelectionBeHighlighted()) {
      onShowBoxModel = this._brieflyShowBoxModel(selection.nodeFront);
    }

    onShow = this.showNode(selection.nodeFront).then(() => {
      // We could be destroyed by now.
      if (this._destroyer) {
        return promise.reject("markupview destroyed");
      }

      // Mark the node as selected.
      this.markNodeAsSelected(selection.nodeFront);

      // Make sure the new selection receives focus so the keyboard can be used.
      this.maybeFocusNewSelection();
    }).catch(e => {
      if (!this._destroyer) {
        console.error(e);
      } else {
        console.warn("Could not mark node as selected, the markup-view was " +
          "destroyed while showing the node.");
      }
    });

    promise.all([onShowBoxModel, onShow]).then(done);
  },

  /**
   * Focus the current node selection's MarkupContainer if the selection
   * happened because the user picked an element using the element picker or
   * browser context menu.
   */
  maybeFocusNewSelection: function() {
    let {reason, nodeFront} = this._inspector.selection;

    if (reason !== "browser-context-menu" &&
        reason !== "picker-node-picked") {
      return;
    }

    this.getContainer(nodeFront).focus();
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

  _onCopy: function (evt) {
    // Ignore copy events from editors
    if (this._isInputOrTextarea(evt.target)) {
      return;
    }

    let selection = this._inspector.selection;
    if (selection.isNode()) {
      this._inspector.copyOuterHTML();
    }
    evt.stopPropagation();
    evt.preventDefault();
  },

  /**
   * Key handling.
   */
  _onKeyDown: function(aEvent) {
    let handled = true;

    // Ignore keystrokes that originated in editors.
    if (this._isInputOrTextarea(aEvent.target)) {
      return;
    }

    switch(aEvent.keyCode) {
      case Ci.nsIDOMKeyEvent.DOM_VK_H:
        if (aEvent.metaKey || aEvent.shiftKey) {
          handled = false;
        } else {
          let node = this._selectedContainer.node;
          if (node.hidden) {
            this.walker.unhideNode(node);
          } else {
            this.walker.hideNode(node);
          }
        }
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_DELETE:
        this.deleteNode(this._selectedContainer.node);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_BACK_SPACE:
        this.deleteNode(this._selectedContainer.node, true);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_HOME:
        let rootContainer = this.getContainer(this._rootNode);
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
   * Check if a node is an input or textarea
   */
  _isInputOrTextarea : function (element) {
    let name = element.tagName.toLowerCase();
    return name === "input" || name === "textarea";
  },

  /**
   * Delete a node from the DOM.
   * This is an undoable action.
   *
   * @param {NodeFront} aNode The node to remove.
   * @param {boolean} moveBackward If set to true, focus the previous sibling,
   *  otherwise the next one.
   */
  deleteNode: function(aNode, moveBackward) {
    if (aNode.isDocumentElement ||
        aNode.nodeType == Ci.nsIDOMNode.DOCUMENT_TYPE_NODE ||
        aNode.isAnonymous) {
      return;
    }

    let container = this.getContainer(aNode);

    // Retain the node so we can undo this...
    this.walker.retainNode(aNode).then(() => {
      let parent = aNode.parentNode();
      let nextSibling = null;
      this.undo.do(() => {
        this.walker.removeNode(aNode).then(siblings => {
          nextSibling = siblings.nextSibling;
          let focusNode = moveBackward ? siblings.previousSibling : nextSibling;

          // If we can't move as the user wants, we move to the other direction.
          // If there is no sibling elements anymore, move to the parent node.
          if (!focusNode) {
            focusNode = nextSibling || siblings.previousSibling || parent;
          }

          if (container.selected) {
            this.navigate(this.getContainer(focusNode));
          }
        });
      }, () => {
        this.walker.insertBefore(aNode, parent, nextSibling);
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
   * @param NodeFront aNode
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
      return this.getContainer(aNode);
    }

    let container;
    let {nodeType, isPseudoElement} = aNode;
    if (aNode === this.walker.rootNode) {
      container = new RootContainer(this, aNode);
      this._elt.appendChild(container.elt);
      this._rootNode = aNode;
    } else if (nodeType == Ci.nsIDOMNode.ELEMENT_NODE && !isPseudoElement) {
      container = new MarkupElementContainer(this, aNode, this._inspector);
    } else if (nodeType == Ci.nsIDOMNode.COMMENT_NODE ||
               nodeType == Ci.nsIDOMNode.TEXT_NODE) {
      container = new MarkupTextContainer(this, aNode, this._inspector);
    } else {
      container = new MarkupReadOnlyContainer(this, aNode, this._inspector);
    }

    if (aFlashNode) {
      container.flashMutation();
    }

    this._containers.set(aNode, container);
    container.childrenDirty = true;

    this._updateChildren(container);

    this._inspector.emit("container-created", container);

    return container;
  },

  /**
   * Mutation observer used for included nodes.
   */
  _mutationObserver: function(aMutations) {
    let requiresLayoutChange = false;

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

      let container = this.getContainer(target);
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
        container.childrenDirty = true;
        // Update the children to take care of changes in the markup view DOM.
        this._updateChildren(container, {flash: true});
      } else if (type === "pseudoClassLock") {
        container.update();
      }
    }

    if (requiresLayoutChange) {
      this._inspector.immediateLayoutChange();
    }
    this._waitForChildren().then((nodes) => {
      if (this._destroyer) {
        console.warn("Could not fully update after markup mutations, " +
          "the markup-view was destroyed while waiting for children.");
        return;
      }
      this._flashMutatedNodes(aMutations);
      this._inspector.emit("markupmutation", aMutations);

      // Since the htmlEditor is absolutely positioned, a mutation may change
      // the location in which it should be shown.
      this.htmlEditor.refresh();
    });
  },

  /**
   * React to display-change events from the walker
   * @param {Array} nodes An array of nodeFronts
   */
  _onDisplayChange: function(nodes) {
    for (let node of nodes) {
      let container = this.getContainer(node);
      if (container) {
        container.isDisplayed = node.isDisplayed;
      }
    }
  },

  /**
   * Given a list of mutations returned by the mutation observer, flash the
   * corresponding containers to attract attention.
   */
  _flashMutatedNodes: function(aMutations) {
    let addedOrEditedContainers = new Set();
    let removedContainers = new Set();

    for (let {type, target, added, removed, newValue} of aMutations) {
      let container = this.getContainer(target);

      if (container) {
        if (type === "characterData") {
          addedOrEditedContainers.add(container);
        } else if (type === "attributes" && newValue === null) {
          // Removed attributes should flash the entire node.
          // New or changed attributes will flash the attribute itself
          // in ElementEditor.flashAttribute.
          addedOrEditedContainers.add(container);
        } else if (type === "childList") {
          // If there has been removals, flash the parent
          if (removed.length) {
            removedContainers.add(container);
          }

          // If there has been additions, flash the nodes if their associated
          // container exist (so if their parent is expanded in the inspector).
          added.forEach(added => {
            let addedContainer = this.getContainer(added);
            if (addedContainer) {
              addedOrEditedContainers.add(addedContainer);

              // The node may be added as a result of an append, in which case
              // it will have been removed from another container first, but in
              // these cases we don't want to flash both the removal and the
              // addition
              removedContainers.delete(container);
            }
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
  showNode: function(aNode, centered=true) {
    let parent = aNode;

    this.importNode(aNode);

    while ((parent = parent.parentNode())) {
      this.importNode(parent);
      this.expandNode(parent);
    }

    return this._waitForChildren().then(() => {
      if (this._destroyer) {
        return promise.reject("markupview destroyed");
      }
      return this._ensureVisible(aNode);
    }).then(() => {
      this.layoutHelpers.scrollIntoViewIfNeeded(this.getContainer(aNode).editor.elt, centered);
    }, e => {
      // Only report this rejection as an error if the panel hasn't been
      // destroyed in the meantime.
      if (!this._destroyer) {
        console.error(e);
      } else {
        console.warn("Could not show the node, the markup-view was destroyed " +
          "while waiting for children");
      }
    });
  },

  /**
   * Expand the container's children.
   */
  _expandContainer: function(aContainer) {
    return this._updateChildren(aContainer, {expand: true}).then(() => {
      if (this._destroyer) {
        console.warn("Could not expand the node, the markup-view was destroyed");
        return;
      }
      aContainer.setExpanded(true);
    });
  },

  /**
   * Expand the node's children.
   */
  expandNode: function(aNode) {
    let container = this.getContainer(aNode);
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
    return this._expandAll(this.getContainer(aNode));
  },

  /**
   * Collapse the node's children.
   */
  collapseNode: function(aNode) {
    let container = this.getContainer(aNode);
    container.setExpanded(false);
  },

  /**
   * Returns either the innerHTML or the outerHTML for a remote node.
   * @param aNode The NodeFront to get the outerHTML / innerHTML for.
   * @param isOuter A boolean that, if true, makes the function return the
   *                outerHTML, otherwise the innerHTML.
   * @returns A promise that will be resolved with the outerHTML / innerHTML.
   */
  _getNodeHTML: function(aNode, isOuter) {
    let walkerPromise = null;

    if (isOuter) {
      walkerPromise = this.walker.outerHTML(aNode);
    } else {
      walkerPromise = this.walker.innerHTML(aNode);
    }

    return walkerPromise.then(longstr => {
      return longstr.string().then(html => {
        longstr.release().then(null, console.error);
        return html;
      });
    });
  },

  /**
   * Retrieve the outerHTML for a remote node.
   * @param aNode The NodeFront to get the outerHTML for.
   * @returns A promise that will be resolved with the outerHTML.
   */
  getNodeOuterHTML: function(aNode) {
    return this._getNodeHTML(aNode, true);
  },

  /**
   * Retrieve the innerHTML for a remote node.
   * @param aNode The NodeFront to get the innerHTML for.
   * @returns A promise that will be resolved with the innerHTML.
   */
  getNodeInnerHTML: function(aNode) {
    return this._getNodeHTML(aNode);
  },

  /**
   * Listen to mutations, expect a given node to be removed and try and select
   * the node that sits at the same place instead.
   * This is useful when changing the outerHTML or the tag name so that the
   * newly inserted node gets selected instead of the one that just got removed.
   */
  reselectOnRemoved: function(removedNode, reason) {
    // Only allow one removed node reselection at a time, so that when there are
    // more than 1 request in parallel, the last one wins.
    this.cancelReselectOnRemoved();

    // Get the removedNode index in its parent node to reselect the right node.
    let isHTMLTag = removedNode.tagName.toLowerCase() === "html";
    let oldContainer = this.getContainer(removedNode);
    let parentContainer = this.getContainer(removedNode.parentNode());
    let childIndex = parentContainer.getChildContainers().indexOf(oldContainer);

    let onMutations = this._removedNodeObserver = (e, mutations) => {
      let isNodeRemovalMutation = false;
      for (let mutation of mutations) {
        let containsRemovedNode = mutation.removed &&
                                  mutation.removed.some(n => n === removedNode);
        if (mutation.type === "childList" && (containsRemovedNode || isHTMLTag)) {
          isNodeRemovalMutation = true;
          break;
        }
      }
      if (!isNodeRemovalMutation) {
        return;
      }

      this._inspector.off("markupmutation", onMutations);
      this._removedNodeObserver = null;

      // Don't select the new node if the user has already changed the current
      // selection.
      if (this._inspector.selection.nodeFront === parentContainer.node ||
          (this._inspector.selection.nodeFront === removedNode && isHTMLTag)) {
        let childContainers = parentContainer.getChildContainers();
        if (childContainers && childContainers[childIndex]) {
          this.markNodeAsSelected(childContainers[childIndex].node, reason);
          if (childContainers[childIndex].hasChildren) {
            this.expandNode(childContainers[childIndex].node);
          }
          this.emit("reselectedonremoved");
        }
      }
    };

    // Start listening for mutations until we find a childList change that has
    // removedNode removed.
    this._inspector.on("markupmutation", onMutations);
  },

  /**
   * Make sure to stop listening for node removal markupmutations and not
   * reselect the corresponding node when that happens.
   * Useful when the outerHTML/tagname edition failed.
   */
  cancelReselectOnRemoved: function() {
    if (this._removedNodeObserver) {
      this._inspector.off("markupmutation", this._removedNodeObserver);
      this._removedNodeObserver = null;
      this.emit("canceledreselectonremoved");
    }
  },

  /**
   * Replace the outerHTML of any node displayed in the inspector with
   * some other HTML code
   * @param {NodeFront} node node which outerHTML will be replaced.
   * @param {string} newValue The new outerHTML to set on the node.
   * @param {string} oldValue The old outerHTML that will be used if the
   *                          user undoes the update.
   * @returns A promise that will resolve when the outer HTML has been updated.
   */
  updateNodeOuterHTML: function(node, newValue, oldValue) {
    let container = this.getContainer(node);
    if (!container) {
      return promise.reject();
    }

    // Changing the outerHTML removes the node which outerHTML was changed.
    // Listen to this removal to reselect the right node afterwards.
    this.reselectOnRemoved(node, "outerhtml");
    return this.walker.setOuterHTML(node, newValue).then(null, () => {
      this.cancelReselectOnRemoved();
    });
  },

  /**
   * Replace the innerHTML of any node displayed in the inspector with
   * some other HTML code
   * @param {Node} node node which innerHTML will be replaced.
   * @param {string} newValue The new innerHTML to set on the node.
   * @param {string} oldValue The old innerHTML that will be used if the user
   *                 undoes the update.
   * @returns A promise that will resolve when the inner HTML has been updated.
   */
  updateNodeInnerHTML: function(node, newValue, oldValue) {
    let container = this.getContainer(node);
    if (!container) {
      return promise.reject();
    }

    let def = promise.defer();

    container.undo.do(() => {
      this.walker.setInnerHTML(node, newValue).then(def.resolve, def.reject);
    }, () => {
      this.walker.setInnerHTML(node, oldValue);
    });

    return def.promise;
  },

  /**
   * Insert adjacent HTML to any node displayed in the inspector.
   *
   * @param {NodeFront} node The reference node.
   * @param {string} position The position as specified for Element.insertAdjacentHTML
   *        (i.e. "beforeBegin", "afterBegin", "beforeEnd", "afterEnd").
   * @param {string} newValue The adjacent HTML.
   * @returns A promise that will resolve when the adjacent HTML has
   *          been inserted.
   */
  insertAdjacentHTMLToNode: function(node, position, value) {
    let container = this.getContainer(node);
    if (!container) {
      return promise.reject();
    }

    let def = promise.defer();

    let injectedNodes = [];
    container.undo.do(() => {
      this.walker.insertAdjacentHTML(node, position, value).then(nodeArray => {
        injectedNodes = nodeArray.nodes;
        return nodeArray;
      }).then(def.resolve, def.reject);
    }, () => {
      this.walker.removeNodes(injectedNodes);
    });

    return def.promise;
  },

  /**
   * Open an editor in the UI to allow editing of a node's outerHTML.
   * @param aNode The NodeFront to edit.
   */
  beginEditingOuterHTML: function(aNode) {
    this.getNodeOuterHTML(aNode).then(oldValue => {
      let container = this.getContainer(aNode);
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
   * @param {NodeFront} aNode The NodeFront to mark as expanded.
   * @param {Boolean} aExpanded Whether the expand or collapse.
   * @param {Boolean} aExpandDescendants Whether to expand all descendants too
   */
  setNodeExpanded: function(aNode, aExpanded, aExpandDescendants) {
    if (aExpanded) {
      if (aExpandDescendants) {
        this.expandAll(aNode);
      } else {
        this.expandNode(aNode);
      }
    } else {
      this.collapseNode(aNode);
    }
  },

  /**
   * Mark the given node selected, and update the inspector.selection
   * object's NodeFront to keep consistent state between UI and selection.
   * @param {NodeFront} aNode The NodeFront to mark as selected.
   * @param {String} reason The reason for marking the node as selected.
   * @return {Boolean} False if the node is already marked as selected, true
   * otherwise.
   */
  markNodeAsSelected: function(node, reason) {
    let container = this.getContainer(node);
    if (this._selectedContainer === container) {
      return false;
    }

    // Un-select the previous container.
    if (this._selectedContainer) {
      this._selectedContainer.selected = false;
    }

    // Select the new container.
    this._selectedContainer = container;
    if (node) {
      this._selectedContainer.selected = true;
    }

    // Change the current selection if needed.
    if (this._inspector.selection.nodeFront !== node) {
      this._inspector.selection.setNodeFront(node, reason || "nodeselected");
    }

    return true;
  },

  /**
   * Make sure that every ancestor of the selection are updated
   * and included in the list of visible children.
   */
  _ensureVisible: function(node) {
    while (node) {
      let container = this.getContainer(node);
      let parent = node.parentNode();
      if (!container.elt.parentNode) {
        let parentContainer = this.getContainer(parent);
        if (parentContainer) {
          parentContainer.childrenDirty = true;
          this._updateChildren(parentContainer, {expand: true});
        }
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

    if (aContainer.singleTextChild
        && aContainer.singleTextChild != aContainer.node.singleTextChild) {

      // This container was doing double duty as a container for a single
      // text child, back that out.
      this._containers.delete(aContainer.singleTextChild);
      aContainer.clearSingleTextChild();

      if (aContainer.hasChildren && aContainer.selected) {
        aContainer.setExpanded(true);
      }
    }

    if (aContainer.node.singleTextChild) {
      aContainer.setExpanded(false);
      // this container will do double duty as the container for the single
      // text child.
      while (aContainer.children.firstChild) {
        aContainer.children.removeChild(aContainer.children.firstChild);
      }

      aContainer.setSingleTextChild(aContainer.node.singleTextChild);

      this._containers.set(aContainer.node.singleTextChild, aContainer);
      aContainer.childrenDirty = false;
      return promise.resolve(aContainer);
    }

    if (!aContainer.hasChildren) {
      while (aContainer.children.firstChild) {
        aContainer.children.removeChild(aContainer.children.firstChild);
      }
      aContainer.childrenDirty = false;
      aContainer.setExpanded(false);
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

    return promise.all([...this._queuedChildUpdates.values()]);
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

    this._destroyer = promise.resolve();

    this._clearBriefBoxModelTimer();

    this._elt.removeEventListener("click", this._onMouseClick, false);

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

    this._frame.contentWindow.removeEventListener("copy", this._onCopy);
    this._onCopy = null;

    this._inspector.selection.off("new-node-front", this._boundOnNewSelection);
    this._boundOnNewSelection = null;

    this.walker.off("mutations", this._boundMutationObserver);
    this._boundMutationObserver = null;

    this.walker.off("display-change", this._boundOnDisplayChange);
    this._boundOnDisplayChange = null;

    this._elt.removeEventListener("mousemove", this._onMouseMove, false);
    this._elt.removeEventListener("mouseleave", this._onMouseLeave, false);
    this._elt = null;

    for (let [key, container] of this._containers) {
      container.destroy();
    }
    this._containers = null;

    this.tooltip.destroy();
    this.tooltip = null;

    this.win = null;
    this.doc = null;

    this._lastDropTarget = null;
    this._lastDragTarget = null;

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

    height = ~~(win.innerHeight * ratio) + "px";
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
    clearTimeout(this._resizePreviewTimeout);

    setTimeout(() => {
      this._updatePreview();
      this._previewBar.classList.remove("hide");
    }, 1000);
  },

  /**
   * Takes an element as it's only argument and marks the element
   * as the drop target
   */
  indicateDropTarget: function(el) {
    if (this._lastDropTarget) {
      this._lastDropTarget.classList.remove("drop-target");
    }

    if (!el) return;

    let target = el.classList.contains("tag-line") ?
                 el : el.querySelector(".tag-line") || el.closest(".tag-line");
    if (!target) return;

    target.classList.add("drop-target");
    this._lastDropTarget = target;
  },

  /**
   * Takes an element to mark it as indicator of dragging target's initial place
   */
  indicateDragTarget: function(el) {
    if (this._lastDragTarget) {
      this._lastDragTarget.classList.remove("drag-target");
    }

    if (!el) return;

    let target = el.classList.contains("tag-line") ?
                 el : el.querySelector(".tag-line") || el.closest(".tag-line");

    if (!target) return;

    target.classList.add("drag-target");
    this._lastDragTarget = target;
  },

  /**
   * Used to get the nodes required to modify the markup after dragging the element (parent/nextSibling)
   */
  get dropTargetNodes() {
    let target = this._lastDropTarget;

    if (!target) {
      return null;
    }

    let parent, nextSibling;

    if (this._lastDropTarget.previousElementSibling &&
        this._lastDropTarget.previousElementSibling.nodeName.toLowerCase() === "ul") {
      parent = target.parentNode.container.node;
      nextSibling = null;
    } else {
      parent = target.parentNode.container.node.parentNode();
      nextSibling = target.parentNode.container.node;
    }

    if (nextSibling && nextSibling.isBeforePseudoElement) {
      nextSibling = target.parentNode.parentNode.children[1].container.node;
    }
    if (nextSibling && nextSibling.isAfterPseudoElement) {
      parent = target.parentNode.container.node.parentNode();
      nextSibling = null;
    }

    if (parent.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE) {
      return null;
    }

    return {parent, nextSibling};
  }
};

/**
 * The main structure for storing a document node in the markup
 * tree.  Manages creation of the editor for the node and
 * a <ul> for placing child elements, and expansion/collapsing
 * of the element.
 *
 * This should not be instantiated directly, instead use one of:
 *    MarkupReadOnlyContainer
 *    MarkupTextContainer
 *    MarkupElementContainer
 */
function MarkupContainer() { }

MarkupContainer.prototype = {

  /*
   * Initialize the MarkupContainer.  Should be called while one
   * of the other contain classes is instantiated.
   *
   * @param MarkupView markupView
   *        The markup view that owns this container.
   * @param NodeFront node
   *        The node to display.
   * @param string templateID
   *        Which template to render for this container
   */
  initialize: function(markupView, node, templateID) {
    this.markup = markupView;
    this.node = node;
    this.undo = this.markup.undo;
    this.win = this.markup._frame.contentWindow;

    // The template will fill the following properties
    this.elt = null;
    this.expander = null;
    this.tagState = null;
    this.tagLine = null;
    this.children = null;
    this.markup.template(templateID, this);
    this.elt.container = this;

    this._onMouseDown = this._onMouseDown.bind(this);
    this._onToggle = this._onToggle.bind(this);
    this._onMouseUp = this._onMouseUp.bind(this);
    this._onMouseMove = this._onMouseMove.bind(this);

    // Binding event listeners
    this.elt.addEventListener("mousedown", this._onMouseDown, false);
    this.markup.doc.body.addEventListener("mouseup", this._onMouseUp, true);
    this.markup.doc.body.addEventListener("mousemove", this._onMouseMove, true);
    this.elt.addEventListener("dblclick", this._onToggle, false);
    if (this.expander) {
      this.expander.addEventListener("click", this._onToggle, false);
    }

    // Marking the node as shown or hidden
    this.isDisplayed = this.node.isDisplayed;
  },

  toString: function() {
    return "[MarkupContainer for " + this.node + "]";
  },

  isPreviewable: function() {
    if (this.node.tagName && !this.node.isPseudoElement) {
      let tagName = this.node.tagName.toLowerCase();
      let srcAttr = this.editor.getAttributeElement("src");
      let isImage = tagName === "img" && srcAttr;
      let isCanvas = tagName === "canvas";

      return isImage || isCanvas;
    } else {
      return false;
    }
  },

  /**
   * Show the element has displayed or not
   */
  set isDisplayed(isDisplayed) {
    this.elt.classList.remove("not-displayed");
    if (!isDisplayed) {
      this.elt.classList.add("not-displayed");
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
    this.updateExpander();
  },

  /**
   * True if the current node can be expanded.
   */
  get canExpand() {
    return this._hasChildren && !this.node.singleTextChild;
  },

  /**
   * True if this is the root <html> element and can't be collapsed
   */
  get mustExpand() {
    return this.node._parent === this.markup.walker.rootNode;
  },

  updateExpander: function() {
    if (!this.expander) {
      return;
    }

    if (this.canExpand && !this.mustExpand) {
      this.expander.style.visibility = "visible";
    } else {
      this.expander.style.visibility = "hidden";
    }
  },

  /**
   * If the node has children, return the list of containers for all these
   * children.
   */
  getChildContainers: function() {
    if (!this.hasChildren) {
      return null;
    }

    return [...this.children.children].map(node => node.container);
  },

  /**
   * True if the node has been visually expanded in the tree.
   */
  get expanded() {
    return !this.elt.classList.contains("collapsed");
  },

  setExpanded: function(aValue) {
    if (!this.expander) {
      return;
    }

    if (!this.canExpand) {
      aValue = false;
    }
    if (this.mustExpand) {
      aValue = true;
    }

    if (aValue && this.elt.classList.contains("collapsed")) {
      // Expanding a node means cloning its "inline" closing tag into a new
      // tag-line that the user can interact with and showing the children.
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

      this.elt.classList.remove("collapsed");
      this.expander.setAttribute("open", "");
      this.hovered = false;
    } else if (!aValue) {
      if (this.closeTagLine) {
        this.elt.removeChild(this.closeTagLine);
        this.closeTagLine = undefined;
      }
      this.elt.classList.add("collapsed");
      this.expander.removeAttribute("open");
    }
  },

  parentContainer: function() {
    return this.elt.parentNode ? this.elt.parentNode.container : null;
  },

  _isMouseDown: false,
  _isDragging: false,
  _dragStartY: 0,

  set isDragging(isDragging) {
    this._isDragging = isDragging;
    this.markup.isDragging = isDragging;

    if (isDragging) {
      this.elt.classList.add("dragging");
      this.markup.doc.body.classList.add("dragging");
    } else {
      this.elt.classList.remove("dragging");
      this.markup.doc.body.classList.remove("dragging");
    }
  },

  get isDragging() {
    return this._isDragging;
  },

  _onMouseDown: function(event) {
    let target = event.target;

    // The "show more nodes" button (already has its onclick).
    if (target.nodeName === "button") {
      return;
    }

    // target is the MarkupContainer itself.
    this._isMouseDown = true;
    this.hovered = false;
    this.markup.navigate(this);
    event.stopPropagation();

    // Preventing the default behavior will avoid the body to gain focus on
    // mouseup (through bubbling) when clicking on a non focusable node in the
    // line. So, if the click happened outside of a focusable element, do
    // prevent the default behavior, so that the tagname or textcontent gains
    // focus.
    if (!target.closest(".editor [tabindex]")) {
      event.preventDefault();
    }

    let isMiddleClick = event.button === 1;
    let isMetaClick = event.button === 0 && (event.metaKey || event.ctrlKey);

    if (isMiddleClick || isMetaClick) {
      let link = target.dataset.link;
      let type = target.dataset.type;
      this.markup._inspector.followAttributeLink(type, link);
      return;
    }

    // Start dragging the container after a delay.
    this.markup._dragStartEl = target;
    setTimeout(() => {
      // Make sure the mouse is still down and on target.
      if (!this._isMouseDown || this.markup._dragStartEl !== target ||
          this.node.isPseudoElement || this.node.isAnonymous ||
          !this.win.getSelection().isCollapsed) {
        return;
      }
      this.isDragging = true;

      this._dragStartY = event.pageY;
      this.markup.indicateDropTarget(this.elt);

      // If this is the last child, use the closing <div.tag-line> of parent as indicator
      this.markup.indicateDragTarget(this.elt.nextElementSibling ||
                                     this.markup.getContainer(this.node.parentNode()).closeTagLine);
    }, GRAB_DELAY);
  },

  /**
   * On mouse up, stop dragging.
   */
  _onMouseUp: Task.async(function*() {
    this._isMouseDown = false;

    if (!this.isDragging) {
      return;
    }

    this.isDragging = false;
    this.elt.style.removeProperty("top");

    let dropTargetNodes = this.markup.dropTargetNodes;

    if (!dropTargetNodes) {
      return;
    }

    yield this.markup.walker.insertBefore(this.node, dropTargetNodes.parent,
                                          dropTargetNodes.nextSibling);
    this.markup.emit("drop-completed");
  }),

  /**
   * On mouse move, move the dragged element if any and indicate the drop target.
   */
  _onMouseMove: function(event) {
    if (!this.isDragging) {
      return;
    }

    let diff = event.pageY - this._dragStartY;
    this.elt.style.top = diff + "px";

    let el = this.markup.doc.elementFromPoint(event.pageX - this.win.scrollX,
                                              event.pageY - this.win.scrollY);

    this.markup.indicateDropTarget(el);
  },

  /**
   * Temporarily flash the container to attract attention.
   * Used for markup mutations.
   */
  flashMutation: function() {
    if (!this.selected) {
      let contentWin = this.win;
      flashElementOn(this.tagState, this.editor.elt);
      if (this._flashMutationTimer) {
        clearTimeout(this._flashMutationTimer);
        this._flashMutationTimer = null;
      }
      this._flashMutationTimer = setTimeout(() => {
        flashElementOff(this.tagState, this.editor.elt);
      }, this.markup.CONTAINER_FLASHING_DURATION);
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
    if (this.node.pseudoClassLocks.length) {
      this.elt.classList.add("pseudoclass-locked");
    } else {
      this.elt.classList.remove("pseudoclass-locked");
    }

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

  _onToggle: function(event) {
    this.markup.navigate(this);
    if (this.hasChildren) {
      this.markup.setNodeExpanded(this.node, !this.expanded, event.altKey);
    }
    event.stopPropagation();
  },

  /**
   * Get rid of event listeners and references, when the container is no longer
   * needed
   */
  destroy: function() {
    // Remove event listeners
    this.elt.removeEventListener("mousedown", this._onMouseDown, false);
    this.elt.removeEventListener("dblclick", this._onToggle, false);
    this.markup.doc.body.removeEventListener("mouseup", this._onMouseUp, true);
    this.markup.doc.body.removeEventListener("mousemove", this._onMouseMove, true);

    this.win = null;

    if (this.expander) {
      this.expander.removeEventListener("click", this._onToggle, false);
    }

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

    this.editor.destroy();
  }
};

/**
 * An implementation of MarkupContainer for Pseudo Elements,
 * Doctype nodes, or any other type generic node that doesn't
 * fit for other editors.
 * Does not allow any editing, just viewing / selecting.
 *
 * @param MarkupView markupView
 *        The markup view that owns this container.
 * @param NodeFront node
 *        The node to display.
 */
function MarkupReadOnlyContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(this, markupView, node, "readonlycontainer");

  this.editor = new GenericEditor(this, node);
  this.tagLine.appendChild(this.editor.elt);
}

MarkupReadOnlyContainer.prototype = Heritage.extend(MarkupContainer.prototype, {});

/**
 * An implementation of MarkupContainer for text node and comment nodes.
 * Allows basic text editing in a textarea.
 *
 * @param MarkupView aMarkupView
 *        The markup view that owns this container.
 * @param NodeFront aNode
 *        The node to display.
 * @param Inspector aInspector
 *        The inspector tool container the markup-view
 */
function MarkupTextContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(this, markupView, node, "textcontainer");

  if (node.nodeType == Ci.nsIDOMNode.TEXT_NODE) {
    this.editor = new TextEditor(this, node, "text");
  } else if (node.nodeType == Ci.nsIDOMNode.COMMENT_NODE) {
    this.editor = new TextEditor(this, node, "comment");
  } else {
    throw "Invalid node for MarkupTextContainer";
  }

  this.tagLine.appendChild(this.editor.elt);
}

MarkupTextContainer.prototype = Heritage.extend(MarkupContainer.prototype, {});

/**
 * An implementation of MarkupContainer for Elements that can contain
 * child nodes.
 * Allows editing of tag name, attributes, expanding / collapsing.
 *
 * @param MarkupView markupView
 *        The markup view that owns this container.
 * @param NodeFront node
 *        The node to display.
 */
function MarkupElementContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(this, markupView, node, "elementcontainer");

  if (node.nodeType === Ci.nsIDOMNode.ELEMENT_NODE) {
    this.editor = new ElementEditor(this, node);
  } else {
    throw "Invalid node for MarkupElementContainer";
  }

  this.tagLine.appendChild(this.editor.elt);
}

MarkupElementContainer.prototype = Heritage.extend(MarkupContainer.prototype, {

  _buildEventTooltipContent: function(target, tooltip) {
    if (target.hasAttribute("data-event")) {
      tooltip.hide(target);

      this.node.getEventListenerInfo().then(listenerInfo => {
        tooltip.setEventContent({
          eventListenerInfos: listenerInfo,
          toolbox: this.markup._inspector.toolbox
        });

        this.markup._makeTooltipPersistent(true);
        tooltip.once("hidden", () => {
          this.markup._makeTooltipPersistent(false);
        });
        tooltip.show(target);
      });
      return true;
    }
  },

  /**
   * Generates the an image preview for this Element. The element must be an
   * image or canvas (@see isPreviewable).
   *
   * @return A Promise that is resolved with an object of form
   * { data, size: { naturalWidth, naturalHeight, resizeRatio } } where
   *   - data is the data-uri for the image preview.
   *   - size contains information about the original image size and if the
   *     preview has been resized.
   *
   * If this element is not previewable or the preview cannot be generated for
   * some reason, the Promise is rejected.
   */
  _getPreview: function() {
    if (!this.isPreviewable()) {
      return promise.reject("_getPreview called on a non-previewable element.");
    }

    if (this.tooltipDataPromise) {
      // A preview request is already pending. Re-use that request.
      return this.tooltipDataPromise;
    }

    let maxDim =
      Services.prefs.getIntPref("devtools.inspector.imagePreviewTooltipSize");

    // Fetch the preview from the server.
    this.tooltipDataPromise = Task.spawn(function*() {
      let preview = yield this.node.getImageData(maxDim);
      let data = yield preview.data.string();

      // Clear the pending preview request. We can't reuse the results later as
      // the preview contents might have changed.
      this.tooltipDataPromise = null;

      return { data, size: preview.size };
    }.bind(this));

    return this.tooltipDataPromise;
  },

  /**
   * Executed by MarkupView._isImagePreviewTarget which is itself called when the
   * mouse hovers over a target in the markup-view.
   * Checks if the target is indeed something we want to have an image tooltip
   * preview over and, if so, inserts content into the tooltip.
   * @return a promise that resolves when the content has been inserted or
   * rejects if no preview is required. This promise is then used by Tooltip.js
   * to decide if/when to show the tooltip
   */
  isImagePreviewTarget: function(target, tooltip) {
    // Is this Element previewable.
    if (!this.isPreviewable()) {
      return promise.reject(false);
    }

    // If the Element has an src attribute, the tooltip is shown when hovering
    // over the src url. If not, the tooltip is shown when hovering over the tag
    // name.
    let src = this.editor.getAttributeElement("src");
    let expectedTarget = src ? src.querySelector(".link") : this.editor.tag;
    if (target !== expectedTarget) {
      return promise.reject(false);
    }

    return this._getPreview().then(({data, size}) => {
      // The preview is ready.
      tooltip.setImageContent(data, size);
    }, () => {
      // Indicate the failure but show the tooltip anyway.
      tooltip.setBrokenImageContent();
    });
  },

  copyImageDataUri: function() {
    // We need to send again a request to gettooltipData even if one was sent for
    // the tooltip, because we want the full-size image
    this.node.getImageData().then(data => {
      data.data.string().then(str => {
        clipboardHelper.copyString(str);
      });
    });
  },

  setSingleTextChild: function(singleTextChild) {
    this.singleTextChild = singleTextChild;
    this.editor.updateTextEditor();
  },

  clearSingleTextChild: function() {
    this.singleTextChild = undefined;
    this.editor.updateTextEditor();
  }
});

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
  destroy: function() {},

  /**
   * If the node has children, return the list of containers for all these
   * children.
   */
  getChildContainers: function() {
    return [...this.children.children].map(node => node.container);
  },

  setExpanded: function(aValue) {}
};

/**
 * Creates an editor for non-editable nodes.
 */
function GenericEditor(aContainer, aNode) {
  this.container = aContainer;
  this.markup = this.container.markup;
  this.template = this.markup.template.bind(this.markup);
  this.elt = null;
  this.template("generic", this);

  if (aNode.isPseudoElement) {
    this.tag.classList.add("theme-fg-color5");
    this.tag.textContent = aNode.isBeforePseudoElement ? "::before" : "::after";
  } else if (aNode.nodeType == Ci.nsIDOMNode.DOCUMENT_TYPE_NODE) {
    this.elt.classList.add("comment");
    this.tag.textContent = aNode.doctypeString;
  } else {
    this.tag.textContent = aNode.nodeName;
  }
}

GenericEditor.prototype = {
  destroy: function() {
    this.elt.remove();
  }
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
  this.container = aContainer;
  this.markup = this.container.markup;
  this.node = aNode;
  this.template = this.markup.template.bind(aTemplate);
  this._selected = false;

  this.markup.template(aTemplate, this);

  editableField({
    element: this.value,
    stopOnReturn: true,
    trigger: "dblclick",
    multiline: true,
    trimOutput: false,
    done: (aVal, aCommit) => {
      if (!aCommit) {
        return;
      }
      this.node.getNodeValue().then(longstr => {
        longstr.string().then(oldValue => {
          longstr.release().then(null, console.error);

          this.container.undo.do(() => {
            this.node.setNodeValue(aVal);
          }, () => {
            this.node.setNodeValue(oldValue);
          });
        });
      });
    }
  });

  this.update();
}

TextEditor.prototype = {
  get selected() {
    return this._selected;
  },

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
      if (this.node.incompleteValue) {
        text += ELLIPSIS;
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
          this.markup.emit("text-expand")
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
  this.container = aContainer;
  this.node = aNode;
  this.markup = this.container.markup;
  this.template = this.markup.template.bind(this.markup);
  this.doc = this.markup.doc;

  this.attrElements = new Map();
  this.animationTimers = {};

  // The templates will fill the following properties
  this.elt = null;
  this.tag = null;
  this.closeTag = null;
  this.attrList = null;
  this.newAttr = null;
  this.closeElt = null;

  // Create the main editor
  this.template("element", this);

  // Make the tag name editable (unless this is a remote node or
  // a document element)
  if (!aNode.isDocumentElement) {
    this.tag.setAttribute("tabindex", "0");
    editableField({
      element: this.tag,
      trigger: "dblclick",
      stopOnReturn: true,
      done: this.onTagEdit.bind(this),
    });
  }

  // Make the new attribute space editable.
  this.newAttr.editMode = editableField({
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
        this.container.undo.do(() => {
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
  this.eventNode.style.display = this.node.hasEventListeners ? "inline-block" : "none";

  this.update();
  this.initialized = true;
}

ElementEditor.prototype = {

  set selected(aValue) {
    if (this.textEditor) {
      this.textEditor.selected = aValue;
    }
  },

  flashAttribute: function(attrName) {
    if (this.animationTimers[attrName]) {
      clearTimeout(this.animationTimers[attrName]);
    }

    flashElementOn(this.getAttributeElement(attrName));

    this.animationTimers[attrName] = setTimeout(() => {
      flashElementOff(this.getAttributeElement(attrName));
    }, this.markup.CONTAINER_FLASHING_DURATION);
  },

  /**
   * Update the state of the editor from the node.
   */
  update: function() {
    let nodeAttributes = this.node.attributes || [];

    // Keep the data model in sync with attributes on the node.
    let currentAttributes = new Set(nodeAttributes.map(a=>a.name));
    for (let name of this.attrElements.keys()) {
      if (!currentAttributes.has(name)) {
        this.removeAttribute(name);
      }
    }

    // Only loop through the current attributes on the node.  Missing
    // attributes have already been removed at this point.
    for (let attr of nodeAttributes) {
      let el = this.attrElements.get(attr.name);
      let valueChanged = el && el.querySelector(".attr-value").textContent !== attr.value;
      let isEditing = el && el.querySelector(".editable").inplaceEditor;
      let canSimplyShowEditor = el && (!valueChanged || isEditing);

      if (canSimplyShowEditor) {
        // Element already exists and doesn't need to be recreated.
        // Just show it (it's hidden by default due to the template).
        el.style.removeProperty("display");
      } else {
        // Create a new editor, because the value of an existing attribute
        // has changed.
        let attribute = this._createAttribute(attr);
        attribute.style.removeProperty("display");

        // Temporarily flash the attribute to highlight the change.
        // But not if this is the first time the editor instance has
        // been created.
        if (this.initialized) {
          this.flashAttribute(attr.name);
        }
      }
    }

    this.updateTextEditor();
  },

  /**
   * Update the inline text editor in case of a single text child node.
   */
  updateTextEditor: function() {
    let node = this.node.singleTextChild;

    if (this.textEditor && this.textEditor.node != node) {
      this.elt.removeChild(this.textEditor.elt);
      this.textEditor = null;
    }

    if (node && !this.textEditor) {
      // Create a text editor added to this editor.
      // This editor won't receive an update automatically, so we rely on
      // child text editors to let us know that we need updating.
      this.textEditor = new TextEditor(this.container, node, "text");
      this.elt.insertBefore(this.textEditor.elt,
                            this.elt.firstChild.nextSibling.nextSibling);
    }

    if (this.textEditor) {
      this.textEditor.update();
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

  /**
   * Remove an attribute from the attrElements object and the DOM
   * @param string attrName The name of the attribute to remove
   */
  removeAttribute: function(attrName) {
    let attr = this.attrElements.get(attrName);
    if (attr) {
      this.attrElements.delete(attrName);
      attr.remove();
    }
  },

  _createAttribute: function(aAttr, aBefore = null) {
    // Create the template editor, which will save some variables here.
    let data = {
      attrName: aAttr.name,
    };
    this.template("attribute", data);
    let {attr, inner, name, val} = data;

    // Double quotes need to be handled specially to prevent DOMParser failing.
    // name="v"a"l"u"e" when editing -> name='v"a"l"u"e"'
    // name="v'a"l'u"e" when editing -> name="v'a&quot;l'u&quot;e"
    let editValueDisplayed = aAttr.value || "";
    let hasDoubleQuote = editValueDisplayed.includes('"');
    let hasSingleQuote = editValueDisplayed.includes("'");
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
    attr.editMode = editableField({
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
        } else if (aEvent && aEvent.target.closest(".attr-value") === val) {
          let length = editValueDisplayed.length;
          let editorLength = aEditor.input.value.length;
          let start = editorLength - (length + 1);
          aEditor.input.setSelectionRange(start, start + length);
        } else {
          aEditor.input.select();
        }
      },
      done: (aVal, aCommit, direction) => {
        if (!aCommit || aVal === initial) {
          return;
        }

        let doMods = this._startModifyingAttributes();
        let undoMods = this._startModifyingAttributes();

        // Remove the attribute stored in this editor and re-add any attributes
        // parsed out of the input element. Restore original attribute if
        // parsing fails.
        try {
          this.refocusOnEdit(aAttr.name, attr, direction);
          this._saveAttribute(aAttr.name, undoMods);
          doMods.removeAttribute(aAttr.name);
          this._applyAttributes(aVal, attr, doMods, undoMods);
          this.container.undo.do(() => {
            doMods.apply();
          }, () => {
            undoMods.apply();
          });
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
      let idNode = this.attrElements.get("id");
      before = idNode ? idNode.nextSibling : this.attrList.firstChild;
    }
    this.attrList.insertBefore(attr, before);

    this.removeAttribute(aAttr.name);
    this.attrElements.set(aAttr.name, attr);

    // Parse the attribute value to detect whether there are linkable parts in
    // it (make sure to pass a complete list of existing attributes to the
    // parseAttribute function, by concatenating aAttr, because this could be a
    // newly added attribute not yet on this.node).
    let attributes = this.node.attributes.filter(({name}) => name !== aAttr.name);
    attributes.push(aAttr);
    let parsedLinksData = parseAttribute(this.node.namespaceURI,
      this.node.tagName, attributes, aAttr.name);

    // Create links in the attribute value, and collapse long attributes if
    // needed.
    let collapse = value => {
      if (value && value.match(COLLAPSE_DATA_URL_REGEX)) {
        return truncateString(value, COLLAPSE_DATA_URL_LENGTH);
      }
      return truncateString(value, COLLAPSE_ATTRIBUTE_LENGTH);
    };

    val.innerHTML = "";
    for (let token of parsedLinksData) {
      if (token.type === "string") {
        val.appendChild(this.doc.createTextNode(collapse(token.value)));
      } else {
        let link = this.doc.createElement("span");
        link.classList.add("link");
        link.setAttribute("data-type", token.type);
        link.setAttribute("data-link", token.value);
        link.textContent = collapse(token.value);
        val.appendChild(link);
      }
    }

    name.textContent = aAttr.name;

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
   * Listen to mutations, and when the attribute list is regenerated
   * try to focus on the attribute after the one that's being edited now.
   * If the attribute order changes, go to the beginning of the attribute list.
   */
  refocusOnEdit: function(attrName, attrNode, direction) {
    // Only allow one refocus on attribute change at a time, so when there's
    // more than 1 request in parallel, the last one wins.
    if (this._editedAttributeObserver) {
      this.markup._inspector.off("markupmutation", this._editedAttributeObserver);
      this._editedAttributeObserver = null;
    }

    let container = this.markup.getContainer(this.node);

    let activeAttrs = [...this.attrList.childNodes].filter(el => el.style.display != "none");
    let attributeIndex = activeAttrs.indexOf(attrNode);

    let onMutations = this._editedAttributeObserver = (e, mutations) => {
      let isDeletedAttribute = false;
      let isNewAttribute = false;
      for (let mutation of mutations) {
        let inContainer = this.markup.getContainer(mutation.target) === container;
        if (!inContainer) {
          continue;
        }

        let isOriginalAttribute = mutation.attributeName === attrName;

        isDeletedAttribute = isDeletedAttribute || isOriginalAttribute && mutation.newValue === null;
        isNewAttribute = isNewAttribute || mutation.attributeName !== attrName;
      }
      let isModifiedOrder = isDeletedAttribute && isNewAttribute;
      this._editedAttributeObserver = null;

      // "Deleted" attributes are merely hidden, so filter them out.
      let visibleAttrs = [...this.attrList.childNodes].filter(el => el.style.display != "none");
      let activeEditor;
      if (visibleAttrs.length > 0) {
        if (!direction) {
          // No direction was given; stay on current attribute.
          activeEditor = visibleAttrs[attributeIndex];
        } else if (isModifiedOrder) {
          // The attribute was renamed, reordering the existing attributes.
          // So let's go to the beginning of the attribute list for consistency.
          activeEditor = visibleAttrs[0];
        } else {
          let newAttributeIndex;
          if (isDeletedAttribute) {
            newAttributeIndex = attributeIndex;
          } else {
            if (direction == Ci.nsIFocusManager.MOVEFOCUS_FORWARD) {
              newAttributeIndex = attributeIndex + 1;
            } else if (direction == Ci.nsIFocusManager.MOVEFOCUS_BACKWARD) {
              newAttributeIndex = attributeIndex - 1;
            }
          }

          // The number of attributes changed (deleted), or we moved through the array
          // so check we're still within bounds.
          if (newAttributeIndex >= 0 && newAttributeIndex <= visibleAttrs.length - 1) {
            activeEditor = visibleAttrs[newAttributeIndex];
          }
        }
      }

      // Either we have no attributes left,
      // or we just edited the last attribute and want to move on.
      if (!activeEditor) {
        activeEditor = this.newAttr;
      }

      // Refocus was triggered by tab or shift-tab.
      // Continue in edit mode.
      if (direction) {
        activeEditor.editMode();
      } else {
        // Refocus was triggered by enter.
        // Exit edit mode (but restore focus).
        let editable = activeEditor === this.newAttr ? activeEditor : activeEditor.querySelector(".editable");
        editable.focus();
      }

      this.markup.emit("refocusedonedit");
    };

    // Start listening for mutations until we find an attributes change
    // that modifies this attribute.
    this.markup._inspector.once("markupmutation", onMutations);
  },

  /**
   * Called when the tag name editor has is done editing.
   */
  onTagEdit: function(newTagName, isCommit) {
    if (!isCommit || newTagName.toLowerCase() === this.node.tagName.toLowerCase() ||
        !("editTagName" in this.markup.walker)) {
      return;
    }

    // Changing the tagName removes the node. Make sure the replacing node gets
    // selected afterwards.
    this.markup.reselectOnRemoved(this.node, "edittagname");
    this.markup.walker.editTagName(this.node, newTagName).then(null, () => {
      // Failed to edit the tag name, cancel the reselection.
      this.markup.cancelReselectOnRemoved();
    });
  },

  destroy: function() {
    for (let key in this.animationTimers) {
      clearTimeout(this.animationTimers[key]);
    }
    this.animationTimers = null;
  }
};

function nodeDocument(node) {
  return node.ownerDocument ||
    (node.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE ? node : null);
}

function truncateString(str, maxLength) {
  if (!str || str.length <= maxLength) {
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

  // Handle bad user inputs by appending a " or ' if it fails to parse without
  // them. Also note that a SVG tag is used to make sure the HTML parser
  // preserves mixed-case attributes
  let el = DOMParser.parseFromString("<svg " + attr + "></svg>", "text/html").body.childNodes[0] ||
           DOMParser.parseFromString("<svg " + attr + "\"></svg>", "text/html").body.childNodes[0] ||
           DOMParser.parseFromString("<svg " + attr + "'></svg>", "text/html").body.childNodes[0];

  let div = doc.createElement("div");
  let attributes = [];
  for (let {name, value} of el.attributes) {
    // Try to set on an element in the document, throws exception on bad input.
    // Prevents InvalidCharacterError - "String contains an invalid character".
    try {
      div.setAttribute(name, value);
      attributes.push({ name, value });
    }
    catch(e) { }
  }

  // Attributes return from DOMParser in reverse order from how they are entered.
  return attributes.reverse();
}

/**
 * Apply a 'flashed' background and foreground color to elements.  Intended
 * to be used with flashElementOff as a way of drawing attention to an element.
 *
 * @param  {Node} backgroundElt
 *         The element to set the highlighted background color on.
 * @param  {Node} foregroundElt
 *         The element to set the matching foreground color on.
 *         Optional.  This will equal backgroundElt if not set.
 */
function flashElementOn(backgroundElt, foregroundElt=backgroundElt) {
  if (!backgroundElt || !foregroundElt) {
    return;
  }

  // Make sure the animation class is not here
  backgroundElt.classList.remove("flash-out");

  // Change the background
  backgroundElt.classList.add("theme-bg-contrast");

  foregroundElt.classList.add("theme-fg-contrast");
  [].forEach.call(
    foregroundElt.querySelectorAll("[class*=theme-fg-color]"),
    span => span.classList.add("theme-fg-contrast")
  );
}

/**
 * Remove a 'flashed' background and foreground color to elements.
 * See flashElementOn.
 *
 * @param  {Node} backgroundElt
 *         The element to reomve the highlighted background color on.
 * @param  {Node} foregroundElt
 *         The element to remove the matching foreground color on.
 *         Optional.  This will equal backgroundElt if not set.
 */
function flashElementOff(backgroundElt, foregroundElt=backgroundElt) {
  if (!backgroundElt || !foregroundElt) {
    return;
  }

  // Add the animation class to smoothly remove the background
  backgroundElt.classList.add("flash-out");

  // Remove the background
  backgroundElt.classList.remove("theme-bg-contrast");

  foregroundElt.classList.remove("theme-fg-contrast");
  [].forEach.call(
    foregroundElt.querySelectorAll("[class*=theme-fg-color]"),
    span => span.classList.remove("theme-fg-contrast")
  );
}

/**
 * Map a number from one range to another.
 */
function map(value, oldMin, oldMax, newMin, newMax) {
  let ratio = oldMax - oldMin;
  if (ratio == 0) {
    return value;
  }
  return newMin + (newMax - newMin) * ((value - oldMin) / ratio);
}

loader.lazyGetter(MarkupView.prototype, "strings", () => Services.strings.createBundle(
  "chrome://browser/locale/devtools/inspector.properties"
));

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});
