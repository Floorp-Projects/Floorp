/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals template */

"use strict";

const {Cc, Cu, Ci} = require("chrome");

// Page size for pageup/pagedown
const PAGE_SIZE = 10;
const DEFAULT_MAX_CHILDREN = 100;
const COLLAPSE_DATA_URL_REGEX = /^data.+base64/;
const COLLAPSE_DATA_URL_LENGTH = 60;
const NEW_SELECTION_HIGHLIGHTER_TIMER = 1000;
const DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE = 50;
const DRAG_DROP_MIN_AUTOSCROLL_SPEED = 5;
const DRAG_DROP_MAX_AUTOSCROLL_SPEED = 15;
const DRAG_DROP_MIN_INITIAL_DISTANCE = 10;
const AUTOCOMPLETE_POPUP_PANEL_ID = "markupview_autoCompletePopup";
const ATTR_COLLAPSE_ENABLED_PREF = "devtools.markup.collapseAttributes";
const ATTR_COLLAPSE_LENGTH_PREF = "devtools.markup.collapseAttributeLength";
const PREVIEW_MAX_DIM_PREF = "devtools.inspector.imagePreviewTooltipSize";

// Contains only void (without end tag) HTML elements
const HTML_VOID_ELEMENTS = [ "area", "base", "br", "col", "command", "embed",
  "hr", "img", "input", "keygen", "link", "meta", "param", "source",
  "track", "wbr" ];

const {UndoStack} = require("devtools/client/shared/undo");
const {editableField, InplaceEditor} =
      require("devtools/client/shared/inplace-editor");
const {HTMLEditor} = require("devtools/client/inspector/markup/html-editor");
const promise = require("promise");
const Services = require("Services");
const {Tooltip} = require("devtools/client/shared/widgets/Tooltip");
const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
const {setImageTooltip, setBrokenImageTooltip} =
      require("devtools/client/shared/widgets/tooltip/ImageTooltipHelper");

const EventEmitter = require("devtools/shared/event-emitter");
const Heritage = require("sdk/core/heritage");
const {parseAttribute} =
      require("devtools/client/shared/node-attribute-parser");
const ELLIPSIS = Services.prefs.getComplexValue("intl.ellipsis",
      Ci.nsIPrefLocalizedString).data;
const {Task} = require("devtools/shared/task");
const {scrollIntoViewIfNeeded} = require("devtools/shared/layout/utils");
const {PrefObserver} = require("devtools/client/styleeditor/utils");
const {KeyShortcuts} = require("devtools/client/shared/key-shortcuts");
const {template} = require("devtools/shared/gcli/templater");
const nodeConstants = require("devtools/shared/dom-node-constants");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

loader.lazyRequireGetter(this, "CSS", "CSS");
loader.lazyGetter(this, "DOMParser", () => {
  return Cc["@mozilla.org/xmlextras/domparser;1"]
    .createInstance(Ci.nsIDOMParser);
});
loader.lazyGetter(this, "AutocompletePopup", () => {
  return require("devtools/client/shared/autocomplete-popup").AutocompletePopup;
});

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

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
 * @param  {Inspector} inspector
 *         The inspector we're watching.
 * @param  {iframe} frame
 *         An iframe in which the caller has kindly loaded markup.xhtml.
 */
function MarkupView(inspector, frame, controllerWindow) {
  this._inspector = inspector;
  this.walker = this._inspector.walker;
  this._frame = frame;
  this.win = this._frame.contentWindow;
  this.doc = this._frame.contentDocument;
  this._elt = this.doc.querySelector("#root");
  this.htmlEditor = new HTMLEditor(this.doc);

  try {
    this.maxChildren = Services.prefs.getIntPref("devtools.markup.pagesize");
  } catch (ex) {
    this.maxChildren = DEFAULT_MAX_CHILDREN;
  }

  this.collapseAttributes =
    Services.prefs.getBoolPref(ATTR_COLLAPSE_ENABLED_PREF);
  this.collapseAttributeLength =
    Services.prefs.getIntPref(ATTR_COLLAPSE_LENGTH_PREF);

  // Creating the popup to be used to show CSS suggestions.
  let options = {
    autoSelect: true,
    theme: "auto",
    // panelId option prevents the markupView autocomplete popup from
    // sharing XUL elements with other views, such as ruleView (see Bug 1191093)
    panelId: AUTOCOMPLETE_POPUP_PANEL_ID
  };
  this.popup = new AutocompletePopup(this.doc.defaultView.parent.document,
                                     options);

  this.undo = new UndoStack();
  this.undo.installController(controllerWindow);

  this._containers = new Map();

  // Binding functions that need to be called in scope.
  this._mutationObserver = this._mutationObserver.bind(this);
  this._onDisplayChange = this._onDisplayChange.bind(this);
  this._onMouseClick = this._onMouseClick.bind(this);
  this._onMouseUp = this._onMouseUp.bind(this);
  this._onNewSelection = this._onNewSelection.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onFocus = this._onFocus.bind(this);
  this._onMouseMove = this._onMouseMove.bind(this);
  this._onMouseLeave = this._onMouseLeave.bind(this);
  this._onToolboxPickerHover = this._onToolboxPickerHover.bind(this);
  this._onCollapseAttributesPrefChange =
    this._onCollapseAttributesPrefChange.bind(this);
  this._isImagePreviewTarget = this._isImagePreviewTarget.bind(this);
  this._onBlur = this._onBlur.bind(this);

  EventEmitter.decorate(this);

  // Listening to various events.
  this._elt.addEventListener("click", this._onMouseClick, false);
  this._elt.addEventListener("mousemove", this._onMouseMove, false);
  this._elt.addEventListener("mouseleave", this._onMouseLeave, false);
  this._elt.addEventListener("blur", this._onBlur, true);
  this.win.addEventListener("mouseup", this._onMouseUp);
  this.win.addEventListener("copy", this._onCopy);
  this._frame.addEventListener("focus", this._onFocus, false);
  this.walker.on("mutations", this._mutationObserver);
  this.walker.on("display-change", this._onDisplayChange);
  this._inspector.selection.on("new-node-front", this._onNewSelection);
  this._inspector.toolbox.on("picker-node-hovered", this._onToolboxPickerHover);

  this._onNewSelection();
  this._initTooltips();

  this._prefObserver = new PrefObserver("devtools.markup");
  this._prefObserver.on(ATTR_COLLAPSE_ENABLED_PREF,
                        this._onCollapseAttributesPrefChange);
  this._prefObserver.on(ATTR_COLLAPSE_LENGTH_PREF,
                        this._onCollapseAttributesPrefChange);

  this._initShortcuts();
}

MarkupView.prototype = {
  /**
   * How long does a node flash when it mutates (in ms).
   */
  CONTAINER_FLASHING_DURATION: 500,

  _selectedContainer: null,

  _initTooltips: function () {
    this.eventDetailsTooltip = new Tooltip(this._inspector.panelDoc);
    this.imagePreviewTooltip = new HTMLTooltip(this._inspector.toolbox,
      {type: "arrow"});
    this._enableImagePreviewTooltip();
  },

  _enableImagePreviewTooltip: function () {
    this.imagePreviewTooltip.startTogglingOnHover(this._elt,
      this._isImagePreviewTarget);
  },

  _disableImagePreviewTooltip: function () {
    this.imagePreviewTooltip.stopTogglingOnHover();
  },

  _onToolboxPickerHover: function (event, nodeFront) {
    this.showNode(nodeFront).then(() => {
      this._showContainerAsHovered(nodeFront);
    }, e => console.error(e));
  },

  isDragging: false,

  _onMouseMove: function (event) {
    let target = event.target;

    // Auto-scroll if we're dragging.
    if (this.isDragging) {
      event.preventDefault();
      this._autoScroll(event);
      return;
    }

    // Show the current container as hovered and highlight it.
    // This requires finding the current MarkupContainer (walking up the DOM).
    while (!target.container) {
      if (target.tagName.toLowerCase() === "body") {
        return;
      }
      target = target.parentNode;
    }

    let container = target.container;
    if (this._hoveredNode !== container.node) {
      if (container.node.nodeType !== nodeConstants.TEXT_NODE) {
        this._showBoxModel(container.node);
      } else {
        this._hideBoxModel();
      }
    }
    this._showContainerAsHovered(container.node);

    this.emit("node-hover");
  },

  /**
   * If focus is moved outside of the markup view document and there is a
   * selected container, make its contents not focusable by a keyboard.
   */
  _onBlur: function (event) {
    if (!this._selectedContainer) {
      return;
    }

    let {relatedTarget} = event;
    if (relatedTarget && relatedTarget.ownerDocument === this.doc) {
      return;
    }

    if (this._selectedContainer) {
      this._selectedContainer.clearFocus();
    }
  },

  /**
   * Executed on each mouse-move while a node is being dragged in the view.
   * Auto-scrolls the view to reveal nodes below the fold to drop the dragged
   * node in.
   */
  _autoScroll: function (event) {
    let docEl = this.doc.documentElement;

    if (this._autoScrollInterval) {
      clearInterval(this._autoScrollInterval);
    }

    // Auto-scroll when the mouse approaches top/bottom edge.
    let fromBottom = docEl.clientHeight - event.pageY + this.win.scrollY;
    let fromTop = event.pageY - this.win.scrollY;

    if (fromBottom <= DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE) {
      // Map our distance from 0-50 to 5-15 range so the speed is kept in a
      // range not too fast, not too slow.
      let speed = map(
        fromBottom,
        0, DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE,
        DRAG_DROP_MIN_AUTOSCROLL_SPEED, DRAG_DROP_MAX_AUTOSCROLL_SPEED);

      this._autoScrollInterval = setInterval(() => {
        docEl.scrollTop -= speed - DRAG_DROP_MAX_AUTOSCROLL_SPEED;
      }, 0);
    }

    if (fromTop <= DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE) {
      let speed = map(
        fromTop,
        0, DRAG_DROP_AUTOSCROLL_EDGE_DISTANCE,
        DRAG_DROP_MIN_AUTOSCROLL_SPEED, DRAG_DROP_MAX_AUTOSCROLL_SPEED);

      this._autoScrollInterval = setInterval(() => {
        docEl.scrollTop += speed - DRAG_DROP_MAX_AUTOSCROLL_SPEED;
      }, 0);
    }
  },

  _onMouseClick: function (event) {
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
      container._buildEventTooltipContent(event.target,
        this.eventDetailsTooltip);
    }
  },

  _onMouseUp: function () {
    this.indicateDropTarget(null);
    this.indicateDragTarget(null);
    if (this._autoScrollInterval) {
      clearInterval(this._autoScrollInterval);
    }
  },

  _onCollapseAttributesPrefChange: function () {
    this.collapseAttributes =
      Services.prefs.getBoolPref(ATTR_COLLAPSE_ENABLED_PREF);
    this.collapseAttributeLength =
      Services.prefs.getIntPref(ATTR_COLLAPSE_LENGTH_PREF);
    this.update();
  },

  cancelDragging: function () {
    if (!this.isDragging) {
      return;
    }

    for (let [, container] of this._containers) {
      if (container.isDragging) {
        container.cancelDragging();
        break;
      }
    }

    this.indicateDropTarget(null);
    this.indicateDragTarget(null);
    if (this._autoScrollInterval) {
      clearInterval(this._autoScrollInterval);
    }
  },

  _hoveredNode: null,

  /**
   * Show a NodeFront's container as being hovered
   *
   * @param  {NodeFront} nodeFront
   *         The node to show as hovered
   */
  _showContainerAsHovered: function (nodeFront) {
    if (this._hoveredNode === nodeFront) {
      return;
    }

    if (this._hoveredNode) {
      this.getContainer(this._hoveredNode).hovered = false;
    }

    this.getContainer(nodeFront).hovered = true;
    this._hoveredNode = nodeFront;
  },

  _onMouseLeave: function () {
    if (this._autoScrollInterval) {
      clearInterval(this._autoScrollInterval);
    }
    if (this.isDragging) {
      return;
    }

    this._hideBoxModel(true);
    if (this._hoveredNode) {
      this.getContainer(this._hoveredNode).hovered = false;
    }
    this._hoveredNode = null;

    this.emit("leave");
  },

  /**
   * Show the box model highlighter on a given node front
   *
   * @param  {NodeFront} nodeFront
   *         The node to show the highlighter for
   * @return {Promise} Resolves when the highlighter for this nodeFront is
   *         shown, taking into account that there could already be highlighter
   *         requests queued up
   */
  _showBoxModel: function (nodeFront) {
    return this._inspector.toolbox.highlighterUtils
      .highlightNodeFront(nodeFront);
  },

  /**
   * Hide the box model highlighter on a given node front
   *
   * @param  {NodeFront} nodeFront
   *         The node to hide the highlighter for
   * @param  {Boolean} forceHide
   *         See toolbox-highlighter-utils/unhighlight
   * @return {Promise} Resolves when the highlighter for this nodeFront is
   *         hidden, taking into account that there could already be highlighter
   *         requests queued up
   */
  _hideBoxModel: function (forceHide) {
    return this._inspector.toolbox.highlighterUtils.unhighlight(forceHide);
  },

  _briefBoxModelTimer: null,

  _clearBriefBoxModelTimer: function () {
    if (this._briefBoxModelTimer) {
      clearTimeout(this._briefBoxModelTimer);
      this._briefBoxModelPromise.resolve();
      this._briefBoxModelPromise = null;
      this._briefBoxModelTimer = null;
    }
  },

  _brieflyShowBoxModel: function (nodeFront) {
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

  template: function (name, dest, options = {stack: "markup.xhtml"}) {
    let node = this.doc.getElementById("template-" + name).cloneNode(true);
    node.removeAttribute("id");
    template(node, dest, options);
    return node;
  },

  /**
   * Get the MarkupContainer object for a given node, or undefined if
   * none exists.
   */
  getContainer: function (node) {
    return this._containers.get(node);
  },

  update: function () {
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
   *
   * @return {Promise} the promise returned by
   *         MarkupElementContainer._isImagePreviewTarget
   */
  _isImagePreviewTarget: Task.async(function* (target) {
    // From the target passed here, let's find the parent MarkupContainer
    // and ask it if the tooltip should be shown
    if (this.isDragging) {
      return false;
    }

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
      return container.isImagePreviewTarget(target, this.imagePreviewTooltip);
    }

    return false;
  }),

  /**
   * Given the known reason, should the current selection be briefly highlighted
   * In a few cases, we don't want to highlight the node:
   * - If the reason is null (used to reset the selection),
   * - if it's "inspector-open" (when the inspector opens up, let's not
   * highlight the default node)
   * - if it's "navigateaway" (since the page is being navigated away from)
   * - if it's "test" (this is a special case for mochitest. In tests, we often
   * need to select elements but don't necessarily want the highlighter to come
   * and go after a delay as this might break test scenarios)
   * We also do not want to start a brief highlight timeout if the node is
   * already being hovered over, since in that case it will already be
   * highlighted.
   */
  _shouldNewSelectionBeHighlighted: function () {
    let reason = this._inspector.selection.reason;
    let unwantedReasons = [
      "inspector-open",
      "navigateaway",
      "nodeselected",
      "test"
    ];
    let isHighlight = this._hoveredNode === this._inspector.selection.nodeFront;
    return !isHighlight && reason && unwantedReasons.indexOf(reason) === -1;
  },

  /**
   * React to new-node-front selection events.
   * Highlights the node if needed, and make sure it is shown and selected in
   * the view.
   */
  _onNewSelection: function () {
    let selection = this._inspector.selection;

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

      // Make sure the new selection is navigated to.
      this.maybeNavigateToNewSelection();
      return undefined;
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
   * Maybe make selected the current node selection's MarkupContainer depending
   * on why the current node got selected.
   */
  maybeNavigateToNewSelection: function () {
    let {reason, nodeFront} = this._inspector.selection;

    // The list of reasons that should lead to navigating to the node.
    let reasonsToNavigate = [
      // If the user picked an element with the element picker.
      "picker-node-picked",
      // If the user selected an element with the browser context menu.
      "browser-context-menu",
      // If the user added a new node by clicking in the inspector toolbar.
      "node-inserted"
    ];

    if (reasonsToNavigate.includes(reason)) {
      this.getContainer(this._rootNode).elt.focus();
      this.navigate(this.getContainer(nodeFront));
    }
  },

  /**
   * Create a TreeWalker to find the next/previous
   * node for selection.
   */
  _selectionWalker: function (start) {
    let walker = this.doc.createTreeWalker(
      start || this._elt,
      Ci.nsIDOMNodeFilter.SHOW_ELEMENT,
      function (element) {
        if (element.container &&
            element.container.elt === element &&
            element.container.visible) {
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
   * Register all key shortcuts.
   */
  _initShortcuts: function () {
    let shortcuts = new KeyShortcuts({
      window: this.win,
    });

    this._onShortcut = this._onShortcut.bind(this);

    // Process localizable keys
    ["markupView.hide.key",
     "markupView.edit.key",
     "markupView.scrollInto.key"].forEach(name => {
       let key = this.strings.GetStringFromName(name);
       shortcuts.on(key, (_, event) => this._onShortcut(name, event));
     });

    // Process generic keys:
    ["Delete", "Backspace", "Home", "Left", "Right", "Up", "Down", "PageUp",
     "PageDown", "Esc", "Enter", "Space"].forEach(key => {
       shortcuts.on(key, this._onShortcut);
     });
  },

  /**
   * Key shortcut listener.
   */
  _onShortcut(name, event) {
    if (this._isInputOrTextarea(event.target)) {
      return;
    }
    switch (name) {
      // Localizable keys
      case "markupView.hide.key": {
        let node = this._selectedContainer.node;
        if (node.hidden) {
          this.walker.unhideNode(node);
        } else {
          this.walker.hideNode(node);
        }
        break;
      }
      case "markupView.edit.key": {
        this.beginEditingOuterHTML(this._selectedContainer.node);
        break;
      }
      case "markupView.scrollInto.key": {
        let selection = this._selectedContainer.node;
        this._inspector.scrollNodeIntoView(selection);
        break;
      }
      // Generic keys
      case "Delete": {
        this.deleteNodeOrAttribute();
        break;
      }
      case "Backspace": {
        this.deleteNodeOrAttribute(true);
        break;
      }
      case "Home": {
        let rootContainer = this.getContainer(this._rootNode);
        this.navigate(rootContainer.children.firstChild.container);
        break;
      }
      case "Left": {
        if (this._selectedContainer.expanded) {
          this.collapseNode(this._selectedContainer.node);
        } else {
          let parent = this._selectionWalker().parentNode();
          if (parent) {
            this.navigate(parent.container);
          }
        }
        break;
      }
      case "Right": {
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
      }
      case "Up": {
        let previousNode = this._selectionWalker().previousNode();
        if (previousNode) {
          this.navigate(previousNode.container);
        }
        break;
      }
      case "Down": {
        let nextNode = this._selectionWalker().nextNode();
        if (nextNode) {
          this.navigate(nextNode.container);
        }
        break;
      }
      case "PageUp": {
        let walker = this._selectionWalker();
        let selection = this._selectedContainer;
        for (let i = 0; i < PAGE_SIZE; i++) {
          let previousNode = walker.previousNode();
          if (!previousNode) {
            break;
          }
          selection = previousNode.container;
        }
        this.navigate(selection);
        break;
      }
      case "PageDown": {
        let walker = this._selectionWalker();
        let selection = this._selectedContainer;
        for (let i = 0; i < PAGE_SIZE; i++) {
          let nextNode = walker.nextNode();
          if (!nextNode) {
            break;
          }
          selection = nextNode.container;
        }
        this.navigate(selection);
        break;
      }
      case "Enter":
      case "Space": {
        if (!this._selectedContainer.canFocus) {
          this._selectedContainer.canFocus = true;
          this._selectedContainer.focus();
        } else {
          // Return early to prevent cancelling the event.
          return;
        }
        break;
      }
      case "Esc": {
        if (this.isDragging) {
          this.cancelDragging();
        } else {
          // Return early to prevent cancelling the event when not
          // dragging, to allow the split console to be toggled.
          return;
        }
        break;
      }
      default:
        console.error("Unexpected markup-view key shortcut", name);
        return;
    }
    // Prevent default for this action
    event.stopPropagation();
    event.preventDefault();
  },

  /**
   * Check if a node is an input or textarea
   */
  _isInputOrTextarea: function (element) {
    let name = element.tagName.toLowerCase();
    return name === "input" || name === "textarea";
  },

  /**
   * If there's an attribute on the current node that's currently focused, then
   * delete this attribute, otherwise delete the node itself.
   *
   * @param  {Boolean} moveBackward
   *         If set to true and if we're deleting the node, focus the previous
   *         sibling after deletion, otherwise the next one.
   */
  deleteNodeOrAttribute: function (moveBackward) {
    let focusedAttribute = this.doc.activeElement
                           ? this.doc.activeElement.closest(".attreditor")
                           : null;
    if (focusedAttribute) {
      // The focused attribute might not be in the current selected container.
      let container = focusedAttribute.closest("li.child").container;
      container.removeAttribute(focusedAttribute.dataset.attr);
    } else {
      this.deleteNode(this._selectedContainer.node, moveBackward);
    }
  },

  /**
   * Delete a node from the DOM.
   * This is an undoable action.
   *
   * @param  {NodeFront} node
   *         The node to remove.
   * @param  {Boolean} moveBackward
   *         If set to true, focus the previous sibling, otherwise the next one.
   */
  deleteNode: function (node, moveBackward) {
    if (node.isDocumentElement ||
        node.nodeType == nodeConstants.DOCUMENT_TYPE_NODE ||
        node.isAnonymous) {
      return;
    }

    let container = this.getContainer(node);

    // Retain the node so we can undo this...
    this.walker.retainNode(node).then(() => {
      let parent = node.parentNode();
      let nextSibling = null;
      this.undo.do(() => {
        this.walker.removeNode(node).then(siblings => {
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
        this.walker.insertBefore(node, parent, nextSibling);
      });
    }).then(null, console.error);
  },

  /**
   * If an editable item is focused, select its container.
   */
  _onFocus: function (event) {
    let parent = event.target;
    while (!parent.container) {
      parent = parent.parentNode;
    }
    if (parent) {
      this.navigate(parent.container);
    }
  },

  /**
   * Handle a user-requested navigation to a given MarkupContainer,
   * updating the inspector's currently-selected node.
   *
   * @param  {MarkupContainer} container
   *         The container we're navigating to.
   */
  navigate: function (container) {
    if (!container) {
      return;
    }

    let node = container.node;
    this.markNodeAsSelected(node, "treepanel");
  },

  /**
   * Make sure a node is included in the markup tool.
   *
   * @param  {NodeFront} node
   *         The node in the content document.
   * @param  {Boolean} flashNode
   *         Whether the newly imported node should be flashed
   * @return {MarkupContainer} The MarkupContainer object for this element.
   */
  importNode: function (node, flashNode) {
    if (!node) {
      return null;
    }

    if (this._containers.has(node)) {
      return this.getContainer(node);
    }

    let container;
    let {nodeType, isPseudoElement} = node;
    if (node === this.walker.rootNode) {
      container = new RootContainer(this, node);
      this._elt.appendChild(container.elt);
      this._rootNode = node;
    } else if (nodeType == nodeConstants.ELEMENT_NODE && !isPseudoElement) {
      container = new MarkupElementContainer(this, node, this._inspector);
    } else if (nodeType == nodeConstants.COMMENT_NODE ||
               nodeType == nodeConstants.TEXT_NODE) {
      container = new MarkupTextContainer(this, node, this._inspector);
    } else {
      container = new MarkupReadOnlyContainer(this, node, this._inspector);
    }

    if (flashNode) {
      container.flashMutation();
    }

    this._containers.set(node, container);
    container.childrenDirty = true;

    this._updateChildren(container);

    this._inspector.emit("container-created", container);

    return container;
  },

  /**
   * Mutation observer used for included nodes.
   */
  _mutationObserver: function (mutations) {
    for (let mutation of mutations) {
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

      if (type === "attributes" && mutation.attributeName === "class") {
        container.updateIsDisplayed();
      }
      if (type === "attributes" || type === "characterData"
        || type === "events" || type === "pseudoClassLock") {
        container.update();
      } else if (type === "childList" || type === "nativeAnonymousChildList") {
        container.childrenDirty = true;
        // Update the children to take care of changes in the markup view DOM
        // and update container (and its subtree) DOM tree depth level for
        // accessibility where necessary.
        this._updateChildren(container, {flash: true}).then(() =>
          container.updateLevel());
      }
    }

    this._waitForChildren().then(() => {
      if (this._destroyer) {
        console.warn("Could not fully update after markup mutations, " +
          "the markup-view was destroyed while waiting for children.");
        return;
      }
      this._flashMutatedNodes(mutations);
      this._inspector.emit("markupmutation", mutations);

      // Since the htmlEditor is absolutely positioned, a mutation may change
      // the location in which it should be shown.
      this.htmlEditor.refresh();
    });
  },

  /**
   * React to display-change events from the walker
   *
   * @param  {Array} nodes
   *         An array of nodeFronts
   */
  _onDisplayChange: function (nodes) {
    for (let node of nodes) {
      let container = this.getContainer(node);
      if (container) {
        container.updateIsDisplayed();
      }
    }
  },

  /**
   * Given a list of mutations returned by the mutation observer, flash the
   * corresponding containers to attract attention.
   */
  _flashMutatedNodes: function (mutations) {
    let addedOrEditedContainers = new Set();
    let removedContainers = new Set();

    for (let {type, target, added, removed, newValue} of mutations) {
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
          added.forEach(node => {
            let addedContainer = this.getContainer(node);
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
  showNode: function (node, centered = true) {
    let parent = node;

    this.importNode(node);

    while ((parent = parent.parentNode())) {
      this.importNode(parent);
      this.expandNode(parent);
    }

    return this._waitForChildren().then(() => {
      if (this._destroyer) {
        return promise.reject("markupview destroyed");
      }
      return this._ensureVisible(node);
    }).then(() => {
      scrollIntoViewIfNeeded(this.getContainer(node).editor.elt, centered);
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
  _expandContainer: function (container) {
    return this._updateChildren(container, {expand: true}).then(() => {
      if (this._destroyer) {
        console.warn("Could not expand the node, the markup-view was " +
          "destroyed");
        return;
      }
      container.setExpanded(true);
    });
  },

  /**
   * Expand the node's children.
   */
  expandNode: function (node) {
    let container = this.getContainer(node);
    this._expandContainer(container);
  },

  /**
   * Expand the entire tree beneath a container.
   *
   * @param  {MarkupContainer} container
   *         The container to expand.
   */
  _expandAll: function (container) {
    return this._expandContainer(container).then(() => {
      let child = container.children.firstChild;
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
   * @param  {DOMNode} node
   *         The node to expand, or null to start from the top.
   */
  expandAll: function (node) {
    node = node || this._rootNode;
    return this._expandAll(this.getContainer(node));
  },

  /**
   * Collapse the node's children.
   */
  collapseNode: function (node) {
    let container = this.getContainer(node);
    container.setExpanded(false);
  },

  /**
   * Returns either the innerHTML or the outerHTML for a remote node.
   *
   * @param  {NodeFront} node
   *         The NodeFront to get the outerHTML / innerHTML for.
   * @param  {Boolean} isOuter
   *         If true, makes the function return the outerHTML,
   *         otherwise the innerHTML.
   * @return {Promise} that will be resolved with the outerHTML / innerHTML.
   */
  _getNodeHTML: function (node, isOuter) {
    let walkerPromise = null;

    if (isOuter) {
      walkerPromise = this.walker.outerHTML(node);
    } else {
      walkerPromise = this.walker.innerHTML(node);
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
   *
   * @param  {NodeFront} node
   *         The NodeFront to get the outerHTML for.
   * @return {Promise} that will be resolved with the outerHTML.
   */
  getNodeOuterHTML: function (node) {
    return this._getNodeHTML(node, true);
  },

  /**
   * Retrieve the innerHTML for a remote node.
   *
   * @param  {NodeFront} node
   *         The NodeFront to get the innerHTML for.
   * @return {Promise} that will be resolved with the innerHTML.
   */
  getNodeInnerHTML: function (node) {
    return this._getNodeHTML(node);
  },

  /**
   * Listen to mutations, expect a given node to be removed and try and select
   * the node that sits at the same place instead.
   * This is useful when changing the outerHTML or the tag name so that the
   * newly inserted node gets selected instead of the one that just got removed.
   */
  reselectOnRemoved: function (removedNode, reason) {
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
        if (mutation.type === "childList" &&
            (containsRemovedNode || isHTMLTag)) {
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
  cancelReselectOnRemoved: function () {
    if (this._removedNodeObserver) {
      this._inspector.off("markupmutation", this._removedNodeObserver);
      this._removedNodeObserver = null;
      this.emit("canceledreselectonremoved");
    }
  },

  /**
   * Replace the outerHTML of any node displayed in the inspector with
   * some other HTML code
   *
   * @param  {NodeFront} node
   *         Node which outerHTML will be replaced.
   * @param  {String} newValue
   *         The new outerHTML to set on the node.
   * @param  {String} oldValue
   *         The old outerHTML that will be used if the user undoes the update.
   * @return {Promise} that will resolve when the outer HTML has been updated.
   */
  updateNodeOuterHTML: function (node, newValue) {
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
   * @param  {Node} node
   *         node which innerHTML will be replaced.
   * @param  {String} newValue
   *         The new innerHTML to set on the node.
   * @param  {String} oldValue
   *         The old innerHTML that will be used if the user undoes the update.
   * @return {Promise} that will resolve when the inner HTML has been updated.
   */
  updateNodeInnerHTML: function (node, newValue, oldValue) {
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
   * @param  {NodeFront} node
   *         The reference node.
   * @param  {String} position
   *         The position as specified for Element.insertAdjacentHTML
   *         (i.e. "beforeBegin", "afterBegin", "beforeEnd", "afterEnd").
   * @param  {String} newValue
   *         The adjacent HTML.
   * @return {Promise} that will resolve when the adjacent HTML has
   *         been inserted.
   */
  insertAdjacentHTMLToNode: function (node, position, value) {
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
   *
   * @param  {NodeFront} node
   *         The NodeFront to edit.
   */
  beginEditingOuterHTML: function (node) {
    this.getNodeOuterHTML(node).then(oldValue => {
      let container = this.getContainer(node);
      if (!container) {
        return;
      }
      this.htmlEditor.show(container.tagLine, oldValue);
      this.htmlEditor.once("popuphidden", (e, commit, value) => {
        // Need to focus the <html> element instead of the frame / window
        // in order to give keyboard focus back to doc (from editor).
        this.doc.documentElement.focus();

        if (commit) {
          this.updateNodeOuterHTML(node, value, oldValue);
        }
      });
    });
  },

  /**
   * Mark the given node expanded.
   *
   * @param  {NodeFront} node
   *         The NodeFront to mark as expanded.
   * @param  {Boolean} expanded
   *         Whether the expand or collapse.
   * @param  {Boolean} expandDescendants
   *         Whether to expand all descendants too
   */
  setNodeExpanded: function (node, expanded, expandDescendants) {
    if (expanded) {
      if (expandDescendants) {
        this.expandAll(node);
      } else {
        this.expandNode(node);
      }
    } else {
      this.collapseNode(node);
    }
  },

  /**
   * Mark the given node selected, and update the inspector.selection
   * object's NodeFront to keep consistent state between UI and selection.
   *
   * @param  {NodeFront} aNode
   *         The NodeFront to mark as selected.
   * @param  {String} reason
   *         The reason for marking the node as selected.
   * @return {Boolean} False if the node is already marked as selected, true
   *         otherwise.
   */
  markNodeAsSelected: function (node, reason) {
    let container = this.getContainer(node);

    if (this._selectedContainer === container) {
      return false;
    }

    // Un-select and remove focus from the previous container.
    if (this._selectedContainer) {
      this._selectedContainer.selected = false;
      this._selectedContainer.clearFocus();
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
  _ensureVisible: function (node) {
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
  unmarkSelectedNode: function () {
    if (this._selectedContainer) {
      this._selectedContainer.selected = false;
      this._selectedContainer = null;
    }
  },

  /**
   * Check if the current selection is a descendent of the container.
   * if so, make sure it's among the visible set for the container,
   * and set the dirty flag if needed.
   *
   * @return The node that should be made visible, if any.
   */
  _checkSelectionVisible: function (container) {
    let centered = null;
    let node = this._inspector.selection.nodeFront;
    while (node) {
      if (node.parentNode() === container.node) {
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
   * @param  {MarkupContainer} container
   *         The markup container whose children need updating
   * @param  {Object} options
   *         Options are {expand:boolean,flash:boolean}
   * @return {Promise} that will be resolved when the children are ready
   *         (which may be immediately).
   */
  _updateChildren: function (container, options) {
    let expand = options && options.expand;
    let flash = options && options.flash;

    container.hasChildren = container.node.hasChildren;
    // Accessibility should either ignore empty children or semantically
    // consider them a group.
    container.setChildrenRole();

    if (!this._queuedChildUpdates) {
      this._queuedChildUpdates = new Map();
    }

    if (this._queuedChildUpdates.has(container)) {
      return this._queuedChildUpdates.get(container);
    }

    if (!container.childrenDirty) {
      return promise.resolve(container);
    }

    if (container.singleTextChild
        && container.singleTextChild != container.node.singleTextChild) {
      // This container was doing double duty as a container for a single
      // text child, back that out.
      this._containers.delete(container.singleTextChild);
      container.clearSingleTextChild();

      if (container.hasChildren && container.selected) {
        container.setExpanded(true);
      }
    }

    if (container.node.singleTextChild) {
      container.setExpanded(false);
      // this container will do double duty as the container for the single
      // text child.
      while (container.children.firstChild) {
        container.children.removeChild(container.children.firstChild);
      }

      container.setSingleTextChild(container.node.singleTextChild);

      this._containers.set(container.node.singleTextChild, container);
      container.childrenDirty = false;
      return promise.resolve(container);
    }

    if (!container.hasChildren) {
      while (container.children.firstChild) {
        container.children.removeChild(container.children.firstChild);
      }
      container.childrenDirty = false;
      container.setExpanded(false);
      return promise.resolve(container);
    }

    // If we're not expanded (or asked to update anyway), we're done for
    // now.  Note that this will leave the childrenDirty flag set, so when
    // expanded we'll refresh the child list.
    if (!(container.expanded || expand)) {
      return promise.resolve(container);
    }

    // We're going to issue a children request, make sure it includes the
    // centered node.
    let centered = this._checkSelectionVisible(container);

    // Children aren't updated yet, but clear the childrenDirty flag anyway.
    // If the dirty flag is re-set while we're fetching we'll need to fetch
    // again.
    container.childrenDirty = false;
    let updatePromise =
      this._getVisibleChildren(container, centered).then(children => {
        if (!this._containers) {
          return promise.reject("markup view destroyed");
        }
        this._queuedChildUpdates.delete(container);

        // If children are dirty, we got a change notification for this node
        // while the request was in progress, we need to do it again.
        if (container.childrenDirty) {
          return this._updateChildren(container, {expand: centered});
        }

        let fragment = this.doc.createDocumentFragment();

        for (let child of children.nodes) {
          let childContainer = this.importNode(child, flash);
          fragment.appendChild(childContainer.elt);
        }

        while (container.children.firstChild) {
          container.children.removeChild(container.children.firstChild);
        }

        if (!(children.hasFirst && children.hasLast)) {
          let nodesCount = container.node.numChildren;
          let showAllString = PluralForm.get(nodesCount,
            this.strings.GetStringFromName("markupView.more.showAll2"));
          let data = {
            showing: this.strings.GetStringFromName("markupView.more.showing"),
            showAll: showAllString.replace("#1", nodesCount),
            allButtonClick: () => {
              container.maxChildren = -1;
              container.childrenDirty = true;
              this._updateChildren(container);
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

        container.children.appendChild(fragment);
        return container;
      }).then(null, console.error);
    this._queuedChildUpdates.set(container, updatePromise);
    return updatePromise;
  },

  _waitForChildren: function () {
    if (!this._queuedChildUpdates) {
      return promise.resolve(undefined);
    }

    return promise.all([...this._queuedChildUpdates.values()]);
  },

  /**
   * Return a list of the children to display for this container.
   */
  _getVisibleChildren: function (container, centered) {
    let maxChildren = container.maxChildren || this.maxChildren;
    if (maxChildren == -1) {
      maxChildren = undefined;
    }

    return this.walker.children(container.node, {
      maxNodes: maxChildren,
      center: centered
    });
  },

  /**
   * Tear down the markup panel.
   */
  destroy: function () {
    if (this._destroyer) {
      return this._destroyer;
    }

    this._destroyer = promise.resolve();

    this._clearBriefBoxModelTimer();

    this._hoveredNode = null;

    this.htmlEditor.destroy();
    this.htmlEditor = null;

    this.undo.destroy();
    this.undo = null;

    this.popup.destroy();
    this.popup = null;

    this._elt.removeEventListener("click", this._onMouseClick, false);
    this._elt.removeEventListener("mousemove", this._onMouseMove, false);
    this._elt.removeEventListener("mouseleave", this._onMouseLeave, false);
    this._elt.removeEventListener("blur", this._onBlur, true);
    this.win.removeEventListener("mouseup", this._onMouseUp);
    this.win.removeEventListener("copy", this._onCopy);
    this._frame.removeEventListener("focus", this._onFocus, false);
    this.walker.off("mutations", this._mutationObserver);
    this.walker.off("display-change", this._onDisplayChange);
    this._inspector.selection.off("new-node-front", this._onNewSelection);
    this._inspector.toolbox.off("picker-node-hovered",
                                this._onToolboxPickerHover);

    this._prefObserver.off(ATTR_COLLAPSE_ENABLED_PREF,
                           this._onCollapseAttributesPrefChange);
    this._prefObserver.off(ATTR_COLLAPSE_LENGTH_PREF,
                           this._onCollapseAttributesPrefChange);
    this._prefObserver.destroy();

    this._elt = null;

    for (let [, container] of this._containers) {
      container.destroy();
    }
    this._containers = null;

    this.eventDetailsTooltip.destroy();
    this.eventDetailsTooltip = null;

    this.imagePreviewTooltip.destroy();
    this.imagePreviewTooltip = null;

    this.win = null;
    this.doc = null;

    this._lastDropTarget = null;
    this._lastDragTarget = null;

    return this._destroyer;
  },

  /**
   * Find the closest element with class tag-line. These are used to indicate
   * drag and drop targets.
   *
   * @param  {DOMNode} el
   * @return {DOMNode}
   */
  findClosestDragDropTarget: function (el) {
    return el.classList.contains("tag-line")
           ? el
           : el.querySelector(".tag-line") || el.closest(".tag-line");
  },

  /**
   * Takes an element as it's only argument and marks the element
   * as the drop target
   */
  indicateDropTarget: function (el) {
    if (this._lastDropTarget) {
      this._lastDropTarget.classList.remove("drop-target");
    }

    if (!el) {
      return;
    }

    let target = this.findClosestDragDropTarget(el);
    if (target) {
      target.classList.add("drop-target");
      this._lastDropTarget = target;
    }
  },

  /**
   * Takes an element to mark it as indicator of dragging target's initial place
   */
  indicateDragTarget: function (el) {
    if (this._lastDragTarget) {
      this._lastDragTarget.classList.remove("drag-target");
    }

    if (!el) {
      return;
    }

    let target = this.findClosestDragDropTarget(el);
    if (target) {
      target.classList.add("drag-target");
      this._lastDragTarget = target;
    }
  },

  /**
   * Used to get the nodes required to modify the markup after dragging the
   * element (parent/nextSibling).
   */
  get dropTargetNodes() {
    let target = this._lastDropTarget;

    if (!target) {
      return null;
    }

    let parent, nextSibling;

    if (target.previousElementSibling &&
        target.previousElementSibling.nodeName.toLowerCase() === "ul") {
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

    if (parent.nodeType !== nodeConstants.ELEMENT_NODE) {
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

/**
 * Unique identifier used to set markup container node id.
 * @type {Number}
 */
let markupContainerID = 0;

MarkupContainer.prototype = {
  /*
   * Initialize the MarkupContainer.  Should be called while one
   * of the other contain classes is instantiated.
   *
   * @param  {MarkupView} markupView
   *         The markup view that owns this container.
   * @param  {NodeFront} node
   *         The node to display.
   * @param  {String} templateID
   *         Which template to render for this container
   */
  initialize: function (markupView, node, templateID) {
    this.markup = markupView;
    this.node = node;
    this.undo = this.markup.undo;
    this.win = this.markup._frame.contentWindow;
    this.id = "treeitem-" + markupContainerID++;

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
    this._onKeyDown = this._onKeyDown.bind(this);

    // Binding event listeners
    this.elt.addEventListener("mousedown", this._onMouseDown, false);
    this.win.addEventListener("mouseup", this._onMouseUp, true);
    this.win.addEventListener("mousemove", this._onMouseMove, true);
    this.elt.addEventListener("dblclick", this._onToggle, false);
    if (this.expander) {
      this.expander.addEventListener("click", this._onToggle, false);
    }

    // Marking the node as shown or hidden
    this.updateIsDisplayed();
  },

  toString: function () {
    return "[MarkupContainer for " + this.node + "]";
  },

  isPreviewable: function () {
    if (this.node.tagName && !this.node.isPseudoElement) {
      let tagName = this.node.tagName.toLowerCase();
      let srcAttr = this.editor.getAttributeElement("src");
      let isImage = tagName === "img" && srcAttr;
      let isCanvas = tagName === "canvas";

      return isImage || isCanvas;
    }

    return false;
  },

  /**
   * Show whether the element is displayed or not
   * If an element has the attribute `display: none` or has been hidden with
   * the H key, it is not displayed (faded in markup view).
   * Otherwise, it is displayed.
   */
  updateIsDisplayed: function () {
    this.elt.classList.remove("not-displayed");
    if (!this.node.isDisplayed || this.node.hidden) {
      this.elt.classList.add("not-displayed");
    }
  },

  /**
   * True if the current node has children. The MarkupView
   * will set this attribute for the MarkupContainer.
   */
  _hasChildren: false,

  get hasChildren() {
    return this._hasChildren;
  },

  set hasChildren(value) {
    this._hasChildren = value;
    this.updateExpander();
  },

  /**
   * A list of all elements with tabindex that are not in container's children.
   */
  get focusableElms() {
    return [...this.tagLine.querySelectorAll("[tabindex]")];
  },

  /**
   * An indicator that the container internals are focusable.
   */
  get canFocus() {
    return this._canFocus;
  },

  /**
   * Toggle focusable state for container internals.
   */
  set canFocus(value) {
    if (this._canFocus === value) {
      return;
    }

    this._canFocus = value;

    if (value) {
      this.tagLine.addEventListener("keydown", this._onKeyDown, true);
      this.focusableElms.forEach(elm => elm.setAttribute("tabindex", "0"));
    } else {
      this.tagLine.removeEventListener("keydown", this._onKeyDown, true);
      // Exclude from tab order.
      this.focusableElms.forEach(elm => elm.setAttribute("tabindex", "-1"));
    }
  },

  /**
   * If conatiner and its contents are focusable, exclude them from tab order,
   * and, if necessary, remove focus.
   */
  clearFocus: function () {
    if (!this.canFocus) {
      return;
    }

    this.canFocus = false;
    let doc = this.markup.doc;

    if (!doc.activeElement || doc.activeElement === doc.body) {
      return;
    }

    let parent = doc.activeElement;

    while (parent && parent !== this.elt) {
      parent = parent.parentNode;
    }

    if (parent) {
      doc.activeElement.blur();
    }
  },

  /**
   * True if the current node can be expanded.
   */
  get canExpand() {
    return this._hasChildren && !this.node.singleTextChild;
  },

  /**
   * True if this is the root <html> element and can't be collapsed.
   */
  get mustExpand() {
    return this.node._parent === this.markup.walker.rootNode;
  },

  /**
   * True if current node can be expanded and collapsed.
   */
  get showExpander() {
    return this.canExpand && !this.mustExpand;
  },

  updateExpander: function () {
    if (!this.expander) {
      return;
    }

    if (this.showExpander) {
      this.expander.style.visibility = "visible";
      // Update accessibility expanded state.
      this.tagLine.setAttribute("aria-expanded", this.expanded);
    } else {
      this.expander.style.visibility = "hidden";
      // No need for accessible expanded state indicator when expander is not
      // shown.
      this.tagLine.removeAttribute("aria-expanded");
    }
  },

  /**
   * If current node has no children, ignore them. Otherwise, consider them a
   * group from the accessibility point of view.
   */
  setChildrenRole: function () {
    this.children.setAttribute("role",
      this.hasChildren ? "group" : "presentation");
  },

  /**
   * Set an appropriate DOM tree depth level for a node and its subtree.
   */
  updateLevel: function () {
    // ARIA level should already be set when container template is rendered.
    let currentLevel = this.tagLine.getAttribute("aria-level");
    let newLevel = this.level;
    if (currentLevel === newLevel) {
      // If level did not change, ignore this node and its subtree.
      return;
    }

    this.tagLine.setAttribute("aria-level", newLevel);
    let childContainers = this.getChildContainers();
    if (childContainers) {
      childContainers.forEach(container => container.updateLevel());
    }
  },

  /**
   * If the node has children, return the list of containers for all these
   * children.
   */
  getChildContainers: function () {
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

  setExpanded: function (value) {
    if (!this.expander) {
      return;
    }

    if (!this.canExpand) {
      value = false;
    }
    if (this.mustExpand) {
      value = true;
    }

    if (value && this.elt.classList.contains("collapsed")) {
      // Expanding a node means cloning its "inline" closing tag into a new
      // tag-line that the user can interact with and showing the children.
      let closingTag = this.elt.querySelector(".close");
      if (closingTag) {
        if (!this.closeTagLine) {
          let line = this.markup.doc.createElement("div");
          line.classList.add("tag-line");
          // Closing tag is not important for accessibility.
          line.setAttribute("role", "presentation");

          let tagState = this.markup.doc.createElement("div");
          tagState.classList.add("tag-state");
          line.appendChild(tagState);

          line.appendChild(closingTag.cloneNode(true));

          flashElementOff(line);
          this.closeTagLine = line;
        }
        this.elt.appendChild(this.closeTagLine);
      }

      this.elt.classList.remove("collapsed");
      this.expander.setAttribute("open", "");
      this.hovered = false;
      this.markup.emit("expanded");
    } else if (!value) {
      if (this.closeTagLine) {
        this.elt.removeChild(this.closeTagLine);
        this.closeTagLine = undefined;
      }
      this.elt.classList.add("collapsed");
      this.expander.removeAttribute("open");
      this.markup.emit("collapsed");
    }
    if (this.showExpander) {
      this.tagLine.setAttribute("aria-expanded", this.expanded);
    }
  },

  parentContainer: function () {
    return this.elt.parentNode ? this.elt.parentNode.container : null;
  },

  /**
   * Determine tree depth level of a given node. This is used to specify ARIA
   * level for node tree items and to give them better semantic context.
   */
  get level() {
    let level = 1;
    let parent = this.node.parentNode();
    while (parent && parent !== this.markup.walker.rootNode) {
      level++;
      parent = parent.parentNode();
    }
    return level;
  },

  _isDragging: false,
  _dragStartY: 0,

  set isDragging(isDragging) {
    let rootElt = this.markup.getContainer(this.markup._rootNode).elt;
    this._isDragging = isDragging;
    this.markup.isDragging = isDragging;
    this.tagLine.setAttribute("aria-grabbed", isDragging);

    if (isDragging) {
      this.elt.classList.add("dragging");
      this.markup.doc.body.classList.add("dragging");
      rootElt.setAttribute("aria-dropeffect", "move");
    } else {
      this.elt.classList.remove("dragging");
      this.markup.doc.body.classList.remove("dragging");
      rootElt.setAttribute("aria-dropeffect", "none");
    }
  },

  get isDragging() {
    return this._isDragging;
  },

  /**
   * Check if element is draggable.
   */
  isDraggable: function () {
    let tagName = this.node.tagName && this.node.tagName.toLowerCase();

    return !this.node.isPseudoElement &&
           !this.node.isAnonymous &&
           !this.node.isDocumentElement &&
           tagName !== "body" &&
           tagName !== "head" &&
           this.win.getSelection().isCollapsed &&
           this.node.parentNode().tagName !== null;
  },

  /**
   * Move keyboard focus to a next/previous focusable element inside container
   * that is not part of its children (only if current focus is on first or last
   * element).
   *
   * @param  {DOMNode} current  currently focused element
   * @param  {Boolean} back     direction
   * @return {DOMNode}          newly focused element if any
   */
  _wrapMoveFocus: function (current, back) {
    let elms = this.focusableElms;
    let next;
    if (back) {
      if (elms.indexOf(current) === 0) {
        next = elms[elms.length - 1];
        next.focus();
      }
    } else if (elms.indexOf(current) === elms.length - 1) {
      next = elms[0];
      next.focus();
    }
    return next;
  },

  _onKeyDown: function (event) {
    let {target, keyCode, shiftKey} = event;
    let isInput = this.markup._isInputOrTextarea(target);

    // Ignore all keystrokes that originated in editors except for when 'Tab' is
    // pressed.
    if (isInput && keyCode !== event.DOM_VK_TAB) {
      return;
    }

    switch (keyCode) {
      case event.DOM_VK_TAB:
        // Only handle 'Tab' if tabbable element is on the edge (first or last).
        if (isInput) {
          // Corresponding tabbable element is editor's next sibling.
          let next = this._wrapMoveFocus(target.nextSibling, shiftKey);
          if (next) {
            event.preventDefault();
            // Keep the editing state if possible.
            if (next._editable) {
              let e = this.markup.doc.createEvent("Event");
              e.initEvent(next._trigger, true, true);
              next.dispatchEvent(e);
            }
          }
        } else {
          let next = this._wrapMoveFocus(target, shiftKey);
          if (next) {
            event.preventDefault();
          }
        }
        break;
      case event.DOM_VK_ESCAPE:
        this.clearFocus();
        this.markup.getContainer(this.markup._rootNode).elt.focus();
        if (this.isDragging) {
          // Escape when dragging is handled by markup view itself.
          return;
        }
        event.preventDefault();
        break;
      default:
        return;
    }
    event.stopPropagation();
  },

  _onMouseDown: function (event) {
    let {target, button, metaKey, ctrlKey} = event;
    let isLeftClick = button === 0;
    let isMiddleClick = button === 1;
    let isMetaClick = isLeftClick && (metaKey || ctrlKey);

    // The "show more nodes" button already has its onclick, so early return.
    if (target.nodeName === "button") {
      return;
    }

    // target is the MarkupContainer itself.
    this.hovered = false;
    this.markup.navigate(this);
    // Make container tabbable descendants tabbable and focus in.
    this.canFocus = true;
    this.focus();
    event.stopPropagation();

    // Preventing the default behavior will avoid the body to gain focus on
    // mouseup (through bubbling) when clicking on a non focusable node in the
    // line. So, if the click happened outside of a focusable element, do
    // prevent the default behavior, so that the tagname or textcontent gains
    // focus.
    if (!target.closest(".editor [tabindex]")) {
      event.preventDefault();
    }

    // Follow attribute links if middle or meta click.
    if (isMiddleClick || isMetaClick) {
      let link = target.dataset.link;
      let type = target.dataset.type;
      // Make container tabbable descendants not tabbable (by default).
      this.canFocus = false;
      this.markup._inspector.followAttributeLink(type, link);
      return;
    }

    // Start node drag & drop (if the mouse moved, see _onMouseMove).
    if (isLeftClick && this.isDraggable()) {
      this._isPreDragging = true;
      this._dragStartY = event.pageY;
    }
  },

  /**
   * On mouse up, stop dragging.
   */
  _onMouseUp: Task.async(function* () {
    this._isPreDragging = false;

    if (this.isDragging) {
      this.cancelDragging();

      let dropTargetNodes = this.markup.dropTargetNodes;

      if (!dropTargetNodes) {
        return;
      }

      yield this.markup.walker.insertBefore(this.node, dropTargetNodes.parent,
                                            dropTargetNodes.nextSibling);
      this.markup.emit("drop-completed");
    }
  }),

  /**
   * On mouse move, move the dragged element and indicate the drop target.
   */
  _onMouseMove: function (event) {
    // If this is the first move after mousedown, only start dragging after the
    // mouse has travelled a few pixels and then indicate the start position.
    let initialDiff = Math.abs(event.pageY - this._dragStartY);
    if (this._isPreDragging && initialDiff >= DRAG_DROP_MIN_INITIAL_DISTANCE) {
      this._isPreDragging = false;
      this.isDragging = true;

      // If this is the last child, use the closing <div.tag-line> of parent as
      // indicator.
      let position = this.elt.nextElementSibling ||
                     this.markup.getContainer(this.node.parentNode())
                                .closeTagLine;
      this.markup.indicateDragTarget(position);
    }

    if (this.isDragging) {
      let x = 0;
      let y = event.pageY - this.win.scrollY;

      // Ensure we keep the dragged element within the markup view.
      if (y < 0) {
        y = 0;
      } else if (y >= this.markup.doc.body.offsetHeight - this.win.scrollY) {
        y = this.markup.doc.body.offsetHeight - this.win.scrollY - 1;
      }

      let diff = y - this._dragStartY + this.win.scrollY;
      this.elt.style.top = diff + "px";

      let el = this.markup.doc.elementFromPoint(x, y);
      this.markup.indicateDropTarget(el);
    }
  },

  cancelDragging: function () {
    if (!this.isDragging) {
      return;
    }

    this._isPreDragging = false;
    this.isDragging = false;
    this.elt.style.removeProperty("top");
  },

  /**
   * Temporarily flash the container to attract attention.
   * Used for markup mutations.
   */
  flashMutation: function () {
    if (!this.selected) {
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
  set hovered(value) {
    this.tagState.classList.remove("flash-out");
    this._hovered = value;
    if (value) {
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

  set selected(value) {
    this.tagState.classList.remove("flash-out");
    this._selected = value;
    this.editor.selected = value;
    // Markup tree item should have accessible selected state.
    this.tagLine.setAttribute("aria-selected", value);
    if (this._selected) {
      this.markup.getContainer(this.markup._rootNode).elt.setAttribute(
        "aria-activedescendant", this.id);
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
  update: function () {
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
  focus: function () {
    // Elements with tabindex of -1 are not focusable.
    let focusable = this.editor.elt.querySelector("[tabindex='0']");
    if (focusable) {
      focusable.focus();
    }
  },

  _onToggle: function (event) {
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
  destroy: function () {
    // Remove event listeners
    this.elt.removeEventListener("mousedown", this._onMouseDown, false);
    this.elt.removeEventListener("dblclick", this._onToggle, false);
    this.tagLine.removeEventListener("keydown", this._onKeyDown, true);
    if (this.win) {
      this.win.removeEventListener("mouseup", this._onMouseUp, true);
      this.win.removeEventListener("mousemove", this._onMouseMove, true);
    }

    this.win = null;

    if (this.expander) {
      this.expander.removeEventListener("click", this._onToggle, false);
    }

    // Recursively destroy children containers
    let firstChild = this.children.firstChild;
    while (firstChild) {
      // Not all children of a container are containers themselves
      // ("show more nodes" button is one example)
      if (firstChild.container) {
        firstChild.container.destroy();
      }
      this.children.removeChild(firstChild);
      firstChild = this.children.firstChild;
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
 * @param  {MarkupView} markupView
 *         The markup view that owns this container.
 * @param  {NodeFront} node
 *         The node to display.
 */
function MarkupReadOnlyContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(this, markupView, node,
    "readonlycontainer");

  this.editor = new GenericEditor(this, node);
  this.tagLine.appendChild(this.editor.elt);
}

MarkupReadOnlyContainer.prototype =
  Heritage.extend(MarkupContainer.prototype, {});

/**
 * An implementation of MarkupContainer for text node and comment nodes.
 * Allows basic text editing in a textarea.
 *
 * @param  {MarkupView} markupView
 *         The markup view that owns this container.
 * @param  {NodeFront} node
 *         The node to display.
 * @param  {Inspector} inspector
 *         The inspector tool container the markup-view
 */
function MarkupTextContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(this, markupView, node,
    "textcontainer");

  if (node.nodeType == nodeConstants.TEXT_NODE) {
    this.editor = new TextEditor(this, node, "text");
  } else if (node.nodeType == nodeConstants.COMMENT_NODE) {
    this.editor = new TextEditor(this, node, "comment");
  } else {
    throw new Error("Invalid node for MarkupTextContainer");
  }

  this.tagLine.appendChild(this.editor.elt);
}

MarkupTextContainer.prototype = Heritage.extend(MarkupContainer.prototype, {});

/**
 * An implementation of MarkupContainer for Elements that can contain
 * child nodes.
 * Allows editing of tag name, attributes, expanding / collapsing.
 *
 * @param  {MarkupView} markupView
 *         The markup view that owns this container.
 * @param  {NodeFront} node
 *         The node to display.
 */
function MarkupElementContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(this, markupView, node,
    "elementcontainer");

  if (node.nodeType === nodeConstants.ELEMENT_NODE) {
    this.editor = new ElementEditor(this, node);
  } else {
    throw new Error("Invalid node for MarkupElementContainer");
  }

  this.tagLine.appendChild(this.editor.elt);
}

MarkupElementContainer.prototype = Heritage.extend(MarkupContainer.prototype, {
  _buildEventTooltipContent: function (target, tooltip) {
    if (target.hasAttribute("data-event")) {
      tooltip.hide(target);

      this.node.getEventListenerInfo().then(listenerInfo => {
        tooltip.setEventContent({
          eventListenerInfos: listenerInfo,
          toolbox: this.markup._inspector.toolbox
        });

        // Disable the image preview tooltip while we display the event details
        this.markup._disableImagePreviewTooltip();
        tooltip.once("hidden", () => {
          // Enable the image preview tooltip after closing the event details
          this.markup._enableImagePreviewTooltip();
        });
        tooltip.show(target);
      });
      return true;
    }
    return undefined;
  },

  /**
   * Generates the an image preview for this Element. The element must be an
   * image or canvas (@see isPreviewable).
   *
   * @return {Promise} that is resolved with an object of form
   *         { data, size: { naturalWidth, naturalHeight, resizeRatio } } where
   *         - data is the data-uri for the image preview.
   *         - size contains information about the original image size and if
   *         the preview has been resized.
   *
   * If this element is not previewable or the preview cannot be generated for
   * some reason, the Promise is rejected.
   */
  _getPreview: function () {
    if (!this.isPreviewable()) {
      return promise.reject("_getPreview called on a non-previewable element.");
    }

    if (this.tooltipDataPromise) {
      // A preview request is already pending. Re-use that request.
      return this.tooltipDataPromise;
    }

    // Fetch the preview from the server.
    this.tooltipDataPromise = Task.spawn(function* () {
      let maxDim = Services.prefs.getIntPref(PREVIEW_MAX_DIM_PREF);
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
   * Executed by MarkupView._isImagePreviewTarget which is itself called when
   * the mouse hovers over a target in the markup-view.
   * Checks if the target is indeed something we want to have an image tooltip
   * preview over and, if so, inserts content into the tooltip.
   *
   * @return {Promise} that resolves when the tooltip content is ready. Resolves
   * true if the tooltip should be displayed, false otherwise.
   */
  isImagePreviewTarget: Task.async(function* (target, tooltip) {
    // Is this Element previewable.
    if (!this.isPreviewable()) {
      return false;
    }

    // If the Element has an src attribute, the tooltip is shown when hovering
    // over the src url. If not, the tooltip is shown when hovering over the tag
    // name.
    let src = this.editor.getAttributeElement("src");
    let expectedTarget = src ? src.querySelector(".link") : this.editor.tag;
    if (target !== expectedTarget) {
      return false;
    }

    try {
      let { data, size } = yield this._getPreview();
      // The preview is ready.
      let options = {
        naturalWidth: size.naturalWidth,
        naturalHeight: size.naturalHeight,
        maxDim: Services.prefs.getIntPref(PREVIEW_MAX_DIM_PREF)
      };

      yield setImageTooltip(tooltip, this.markup.doc, data, options);
    } catch (e) {
      // Indicate the failure but show the tooltip anyway.
      yield setBrokenImageTooltip(tooltip, this.markup.doc);
    }
    return true;
  }),

  copyImageDataUri: function () {
    // We need to send again a request to gettooltipData even if one was sent
    // for the tooltip, because we want the full-size image
    this.node.getImageData().then(data => {
      data.data.string().then(str => {
        clipboardHelper.copyString(str);
      });
    });
  },

  setSingleTextChild: function (singleTextChild) {
    this.singleTextChild = singleTextChild;
    this.editor.updateTextEditor();
  },

  clearSingleTextChild: function () {
    this.singleTextChild = undefined;
    this.editor.updateTextEditor();
  },

  /**
   * Trigger new attribute field for input.
   */
  addAttribute: function () {
    this.editor.newAttr.editMode();
  },

  /**
   * Trigger attribute field for editing.
   */
  editAttribute: function (attrName) {
    this.editor.attrElements.get(attrName).editMode();
  },

  /**
   * Remove attribute from container.
   * This is an undoable action.
   */
  removeAttribute: function (attrName) {
    let doMods = this.editor._startModifyingAttributes();
    let undoMods = this.editor._startModifyingAttributes();
    this.editor._saveAttribute(attrName, undoMods);
    doMods.removeAttribute(attrName);
    this.undo.do(() => {
      doMods.apply();
    }, () => {
      undoMods.apply();
    });
  }
});

/**
 * Dummy container node used for the root document element.
 */
function RootContainer(markupView, node) {
  this.doc = markupView.doc;
  this.elt = this.doc.createElement("ul");
  // Root container has tree semantics for accessibility.
  this.elt.setAttribute("role", "tree");
  this.elt.setAttribute("tabindex", "0");
  this.elt.setAttribute("aria-dropeffect", "none");
  this.elt.container = this;
  this.children = this.elt;
  this.node = node;
  this.toString = () => "[root container]";
}

RootContainer.prototype = {
  hasChildren: true,
  expanded: true,
  update: function () {},
  destroy: function () {},

  /**
   * If the node has children, return the list of containers for all these
   * children.
   */
  getChildContainers: function () {
    return [...this.children.children].map(node => node.container);
  },

  /**
   * Set the expanded state of the container node.
   * @param  {Boolean} value
   */
  setExpanded: function () {},

  /**
   * Set an appropriate role of the container's children node.
   */
  setChildrenRole: function () {},

  /**
   * Set an appropriate DOM tree depth level for a node and its subtree.
   */
  updateLevel: function () {}
};

/**
 * Creates an editor for non-editable nodes.
 */
function GenericEditor(container, node) {
  this.container = container;
  this.markup = this.container.markup;
  this.template = this.markup.template.bind(this.markup);
  this.elt = null;
  this.template("generic", this);

  if (node.isPseudoElement) {
    this.tag.classList.add("theme-fg-color5");
    this.tag.textContent = node.isBeforePseudoElement ? "::before" : "::after";
  } else if (node.nodeType == nodeConstants.DOCUMENT_TYPE_NODE) {
    this.elt.classList.add("comment");
    this.tag.textContent = node.doctypeString;
  } else {
    this.tag.textContent = node.nodeName;
  }
}

GenericEditor.prototype = {
  destroy: function () {
    this.elt.remove();
  },

  /**
   * Stub method for consistency with ElementEditor.
   */
  getInfoAtNode: function () {
    return null;
  }
};

/**
 * Creates a simple text editor node, used for TEXT and COMMENT
 * nodes.
 *
 * @param  {MarkupContainer} container
 *         The container owning this editor.
 * @param  {DOMNode} node
 *         The node being edited.
 * @param  {String} templateId
 *         The template id to use to build the editor.
 */
function TextEditor(container, node, templateId) {
  this.container = container;
  this.markup = this.container.markup;
  this.node = node;
  this.template = this.markup.template.bind(templateId);
  this._selected = false;

  this.markup.template(templateId, this);

  editableField({
    element: this.value,
    stopOnReturn: true,
    trigger: "dblclick",
    multiline: true,
    maxWidth: () => {
      let elementRect = this.value.getBoundingClientRect();
      let containerRect = this.container.elt.getBoundingClientRect();
      return containerRect.right - elementRect.left - 2;
    },
    trimOutput: false,
    done: (val, commit) => {
      if (!commit) {
        return;
      }
      this.node.getNodeValue().then(longstr => {
        longstr.string().then(oldValue => {
          longstr.release().then(null, console.error);

          this.container.undo.do(() => {
            this.node.setNodeValue(val);
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

  set selected(value) {
    if (value === this._selected) {
      return;
    }
    this._selected = value;
    this.update();
  },

  update: function () {
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
          this.markup.emit("text-expand");
        }
      }).then(null, console.error);
    }
  },

  destroy: function () {},

  /**
   * Stub method for consistency with ElementEditor.
   */
  getInfoAtNode: function () {
    return null;
  }
};

/**
 * Creates an editor for an Element node.
 *
 * @param  {MarkupContainer} container
 *         The container owning this editor.
 * @param  {Element} node
 *         The node being edited.
 */
function ElementEditor(container, node) {
  this.container = container;
  this.node = node;
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
  if (!node.isDocumentElement) {
    // Make the tag optionally tabbable but not by default.
    this.tag.setAttribute("tabindex", "-1");
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
    done: (val, commit) => {
      if (!commit) {
        return;
      }

      let doMods = this._startModifyingAttributes();
      let undoMods = this._startModifyingAttributes();
      this._applyAttributes(val, null, doMods, undoMods);
      this.container.undo.do(() => {
        doMods.apply();
      }, function () {
        undoMods.apply();
      });
    }
  });

  let displayName = this.node.displayName;
  this.tag.textContent = displayName;
  this.closeTag.textContent = displayName;

  let isVoidElement = HTML_VOID_ELEMENTS.includes(displayName);
  if (node.isInHTMLDocument && isVoidElement) {
    this.elt.classList.add("void-element");
  }

  this.update();
  this.initialized = true;
}

ElementEditor.prototype = {
  set selected(value) {
    if (this.textEditor) {
      this.textEditor.selected = value;
    }
  },

  flashAttribute: function (attrName) {
    if (this.animationTimers[attrName]) {
      clearTimeout(this.animationTimers[attrName]);
    }

    flashElementOn(this.getAttributeElement(attrName));

    this.animationTimers[attrName] = setTimeout(() => {
      flashElementOff(this.getAttributeElement(attrName));
    }, this.markup.CONTAINER_FLASHING_DURATION);
  },

  /**
   * Returns information about node in the editor.
   *
   * @param  {DOMNode} node
   *         The node to get information from.
   * @return {Object} An object literal with the following information:
   *         {type: "attribute", name: "rel", value: "index", el: node}
   */
  getInfoAtNode: function (node) {
    if (!node) {
      return null;
    }

    let type = null;
    let name = null;
    let value = null;

    // Attribute
    let attribute = node.closest(".attreditor");
    if (attribute) {
      type = "attribute";
      name = attribute.querySelector(".attr-name").textContent;
      value = attribute.querySelector(".attr-value").textContent;
    }

    return {type, name, value, el: node};
  },

  /**
   * Update the state of the editor from the node.
   */
  update: function () {
    let nodeAttributes = this.node.attributes || [];

    // Keep the data model in sync with attributes on the node.
    let currentAttributes = new Set(nodeAttributes.map(a => a.name));
    for (let name of this.attrElements.keys()) {
      if (!currentAttributes.has(name)) {
        this.removeAttribute(name);
      }
    }

    // Only loop through the current attributes on the node.  Missing
    // attributes have already been removed at this point.
    for (let attr of nodeAttributes) {
      let el = this.attrElements.get(attr.name);
      let valueChanged = el &&
        el.dataset.value !== attr.value;
      let isEditing = el && el.querySelector(".editable").inplaceEditor;
      let canSimplyShowEditor = el && (!valueChanged || isEditing);

      if (canSimplyShowEditor) {
        // Element already exists and doesn't need to be recreated.
        // Just show it (it's hidden by default due to the template).
        el.style.removeProperty("display");
      } else {
        // Create a new editor, because the value of an existing attribute
        // has changed.
        let attribute = this._createAttribute(attr, el);
        attribute.style.removeProperty("display");

        // Temporarily flash the attribute to highlight the change.
        // But not if this is the first time the editor instance has
        // been created.
        if (this.initialized) {
          this.flashAttribute(attr.name);
        }
      }
    }

    // Update the event bubble display
    this.eventNode.style.display = this.node.hasEventListeners ?
      "inline-block" : "none";

    this.updateTextEditor();
  },

  /**
   * Update the inline text editor in case of a single text child node.
   */
  updateTextEditor: function () {
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

  _startModifyingAttributes: function () {
    return this.node.startModifyingAttributes();
  },

  /**
   * Get the element used for one of the attributes of this element.
   *
   * @param  {String} attrName
   *         The name of the attribute to get the element for
   * @return {DOMNode}
   */
  getAttributeElement: function (attrName) {
    return this.attrList.querySelector(
      ".attreditor[data-attr=" + CSS.escape(attrName) + "] .attr-value");
  },

  /**
   * Remove an attribute from the attrElements object and the DOM.
   *
   * @param  {String} attrName
   *         The name of the attribute to remove
   */
  removeAttribute: function (attrName) {
    let attr = this.attrElements.get(attrName);
    if (attr) {
      this.attrElements.delete(attrName);
      attr.remove();
    }
  },

  _createAttribute: function (attribute, before = null) {
    // Create the template editor, which will save some variables here.
    let data = {
      attrName: attribute.name,
    };
    this.template("attribute", data);
    let {attr, inner, name, val} = data;

    // Double quotes need to be handled specially to prevent DOMParser failing.
    // name="v"a"l"u"e" when editing -> name='v"a"l"u"e"'
    // name="v'a"l'u"e" when editing -> name="v'a&quot;l'u&quot;e"
    let editValueDisplayed = attribute.value || "";
    let hasDoubleQuote = editValueDisplayed.includes('"');
    let hasSingleQuote = editValueDisplayed.includes("'");
    let initial = attribute.name + '="' + editValueDisplayed + '"';

    // Can't just wrap value with ' since the value contains both " and '.
    if (hasDoubleQuote && hasSingleQuote) {
      editValueDisplayed = editValueDisplayed.replace(/\"/g, "&quot;");
      initial = attribute.name + '="' + editValueDisplayed + '"';
    }

    // Wrap with ' since there are no single quotes in the attribute value.
    if (hasDoubleQuote && !hasSingleQuote) {
      initial = attribute.name + "='" + editValueDisplayed + "'";
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
      start: (editor, event) => {
        // If the editing was started inside the name or value areas,
        // select accordingly.
        if (event && event.target === name) {
          editor.input.setSelectionRange(0, name.textContent.length);
        } else if (event && event.target.closest(".attr-value") === val) {
          let length = editValueDisplayed.length;
          let editorLength = editor.input.value.length;
          let start = editorLength - (length + 1);
          editor.input.setSelectionRange(start, start + length);
        } else {
          editor.input.select();
        }
      },
      done: (newValue, commit, direction) => {
        if (!commit || newValue === initial) {
          return;
        }

        let doMods = this._startModifyingAttributes();
        let undoMods = this._startModifyingAttributes();

        // Remove the attribute stored in this editor and re-add any attributes
        // parsed out of the input element. Restore original attribute if
        // parsing fails.
        this.refocusOnEdit(attribute.name, attr, direction);
        this._saveAttribute(attribute.name, undoMods);
        doMods.removeAttribute(attribute.name);
        this._applyAttributes(newValue, attr, doMods, undoMods);
        this.container.undo.do(() => {
          doMods.apply();
        }, () => {
          undoMods.apply();
        });
      }
    });

    // Figure out where we should place the attribute.
    if (attribute.name == "id") {
      before = this.attrList.firstChild;
    } else if (attribute.name == "class") {
      let idNode = this.attrElements.get("id");
      before = idNode ? idNode.nextSibling : this.attrList.firstChild;
    }
    this.attrList.insertBefore(attr, before);

    this.removeAttribute(attribute.name);
    this.attrElements.set(attribute.name, attr);

    // Parse the attribute value to detect whether there are linkable parts in
    // it (make sure to pass a complete list of existing attributes to the
    // parseAttribute function, by concatenating attribute, because this could
    // be a newly added attribute not yet on this.node).
    let attributes = this.node.attributes.filter(existingAttribute => {
      return existingAttribute.name !== attribute.name;
    });
    attributes.push(attribute);
    let parsedLinksData = parseAttribute(this.node.namespaceURI,
      this.node.tagName, attributes, attribute.name);

    // Create links in the attribute value, and collapse long attributes if
    // needed.
    let collapse = value => {
      if (value && value.match(COLLAPSE_DATA_URL_REGEX)) {
        return truncateString(value, COLLAPSE_DATA_URL_LENGTH);
      }
      return this.markup.collapseAttributes
        ? truncateString(value, this.markup.collapseAttributeLength)
        : value;
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

    name.textContent = attribute.name;

    return attr;
  },

  /**
   * Parse a user-entered attribute string and apply the resulting
   * attributes to the node. This operation is undoable.
   *
   * @param  {String} value
   *         The user-entered value.
   * @param  {DOMNode} attrNode
   *         The attribute editor that created this
   *         set of attributes, used to place new attributes where the
   *         user put them.
   */
  _applyAttributes: function (value, attrNode, doMods, undoMods) {
    let attrs = parseAttributeValues(value, this.doc);
    for (let attr of attrs) {
      // Create an attribute editor next to the current attribute if needed.
      this._createAttribute(attr, attrNode ? attrNode.nextSibling : null);
      this._saveAttribute(attr.name, undoMods);
      doMods.setAttribute(attr.name, attr.value);
    }
  },

  /**
   * Saves the current state of the given attribute into an attribute
   * modification list.
   */
  _saveAttribute: function (name, undoMods) {
    let node = this.node;
    if (node.hasAttribute(name)) {
      let oldValue = node.getAttribute(name);
      undoMods.setAttribute(name, oldValue);
    } else {
      undoMods.removeAttribute(name);
    }
  },

  /**
   * Listen to mutations, and when the attribute list is regenerated
   * try to focus on the attribute after the one that's being edited now.
   * If the attribute order changes, go to the beginning of the attribute list.
   */
  refocusOnEdit: function (attrName, attrNode, direction) {
    // Only allow one refocus on attribute change at a time, so when there's
    // more than 1 request in parallel, the last one wins.
    if (this._editedAttributeObserver) {
      this.markup._inspector.off("markupmutation",
        this._editedAttributeObserver);
      this._editedAttributeObserver = null;
    }

    let container = this.markup.getContainer(this.node);

    let activeAttrs = [...this.attrList.childNodes]
      .filter(el => el.style.display != "none");
    let attributeIndex = activeAttrs.indexOf(attrNode);

    let onMutations = this._editedAttributeObserver = (e, mutations) => {
      let isDeletedAttribute = false;
      let isNewAttribute = false;

      for (let mutation of mutations) {
        let inContainer =
          this.markup.getContainer(mutation.target) === container;
        if (!inContainer) {
          continue;
        }

        let isOriginalAttribute = mutation.attributeName === attrName;

        isDeletedAttribute = isDeletedAttribute || isOriginalAttribute &&
                             mutation.newValue === null;
        isNewAttribute = isNewAttribute || mutation.attributeName !== attrName;
      }

      let isModifiedOrder = isDeletedAttribute && isNewAttribute;
      this._editedAttributeObserver = null;

      // "Deleted" attributes are merely hidden, so filter them out.
      let visibleAttrs = [...this.attrList.childNodes]
        .filter(el => el.style.display != "none");
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
          } else if (direction == Ci.nsIFocusManager.MOVEFOCUS_FORWARD) {
            newAttributeIndex = attributeIndex + 1;
          } else if (direction == Ci.nsIFocusManager.MOVEFOCUS_BACKWARD) {
            newAttributeIndex = attributeIndex - 1;
          }

          // The number of attributes changed (deleted), or we moved through
          // the array so check we're still within bounds.
          if (newAttributeIndex >= 0 &&
              newAttributeIndex <= visibleAttrs.length - 1) {
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
        let editable = activeEditor === this.newAttr ?
          activeEditor : activeEditor.querySelector(".editable");
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
  onTagEdit: function (newTagName, isCommit) {
    if (!isCommit ||
        newTagName.toLowerCase() === this.node.tagName.toLowerCase() ||
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

  destroy: function () {
    for (let key in this.animationTimers) {
      clearTimeout(this.animationTimers[key]);
    }
    this.animationTimers = null;
  }
};

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

  let parseAndGetNode = str => {
    return DOMParser.parseFromString(str, "text/html").body.childNodes[0];
  };

  // Handle bad user inputs by appending a " or ' if it fails to parse without
  // them. Also note that a SVG tag is used to make sure the HTML parser
  // preserves mixed-case attributes
  let el = parseAndGetNode("<svg " + attr + "></svg>") ||
           parseAndGetNode("<svg " + attr + "\"></svg>") ||
           parseAndGetNode("<svg " + attr + "'></svg>");

  let div = doc.createElement("div");
  let attributes = [];
  for (let {name, value} of el.attributes) {
    // Try to set on an element in the document, throws exception on bad input.
    // Prevents InvalidCharacterError - "String contains an invalid character".
    try {
      div.setAttribute(name, value);
      attributes.push({ name, value });
    } catch (e) {
      // This may throw exceptions on bad input.
      // Prevents InvalidCharacterError - "String contains an invalid
      // character".
    }
  }

  return attributes;
}

/**
 * Apply a 'flashed' background and foreground color to elements. Intended
 * to be used with flashElementOff as a way of drawing attention to an element.
 *
 * @param  {Node} backgroundElt
 *         The element to set the highlighted background color on.
 * @param  {Node} foregroundElt
 *         The element to set the matching foreground color on.
 *         Optional.  This will equal backgroundElt if not set.
 */
function flashElementOn(backgroundElt, foregroundElt = backgroundElt) {
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
function flashElementOff(backgroundElt, foregroundElt = backgroundElt) {
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
  "chrome://devtools/locale/inspector.properties"
));

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function () {
  return Cc["@mozilla.org/widget/clipboardhelper;1"]
    .getService(Ci.nsIClipboardHelper);
});

exports.MarkupView = MarkupView;
