/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Task} = require("devtools/shared/task");
const {KeyCodes} = require("devtools/client/shared/keycodes");
const {flashElementOn, flashElementOff} =
      require("devtools/client/inspector/markup/utils");

const DRAG_DROP_MIN_INITIAL_DISTANCE = 10;

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
   * @param  {String} type
   *         The type of container to build. This can be either 'textcontainer',
   *         'readonlycontainer' or 'elementcontainer'.
   */
  initialize: function (markupView, node, type) {
    this.markup = markupView;
    this.node = node;
    this.undo = this.markup.undo;
    this.win = this.markup._frame.contentWindow;
    this.id = "treeitem-" + markupContainerID++;
    this.htmlElt = this.win.document.documentElement;

    this.buildMarkup(type);

    this.elt.container = this;

    this._onMouseDown = this._onMouseDown.bind(this);
    this._onToggle = this._onToggle.bind(this);
    this._onMouseUp = this._onMouseUp.bind(this);
    this._onMouseMove = this._onMouseMove.bind(this);
    this._onKeyDown = this._onKeyDown.bind(this);

    // Binding event listeners
    this.elt.addEventListener("mousedown", this._onMouseDown);
    this.win.addEventListener("mouseup", this._onMouseUp, true);
    this.win.addEventListener("mousemove", this._onMouseMove, true);
    this.elt.addEventListener("dblclick", this._onToggle);
    if (this.expander) {
      this.expander.addEventListener("click", this._onToggle);
    }

    // Marking the node as shown or hidden
    this.updateIsDisplayed();
  },

  buildMarkup: function (type) {
    this.elt = this.win.document.createElement("li");
    this.elt.classList.add("child", "collapsed");
    this.elt.setAttribute("role", "presentation");

    this.tagLine = this.win.document.createElement("div");
    this.tagLine.setAttribute("id", this.id);
    this.tagLine.classList.add("tag-line");
    this.tagLine.setAttribute("role", "treeitem");
    this.tagLine.setAttribute("aria-level", this.level);
    this.tagLine.setAttribute("aria-grabbed", this.isDragging);
    this.elt.appendChild(this.tagLine);

    this.tagState = this.win.document.createElement("span");
    this.tagState.classList.add("tag-state");
    this.tagState.setAttribute("role", "presentation");
    this.tagLine.appendChild(this.tagState);

    if (type !== "textcontainer") {
      this.expander = this.win.document.createElement("span");
      this.expander.classList.add("theme-twisty", "expander");
      this.expander.setAttribute("role", "presentation");
      this.tagLine.appendChild(this.expander);
    }

    this.children = this.win.document.createElement("ul");
    this.children.classList.add("children");
    this.children.setAttribute("role", "group");
    this.elt.appendChild(this.children);
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
    return this._hasChildren && !this.node.inlineTextChild;
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
      this.elt.classList.add("expandable");
      this.expander.style.visibility = "visible";
      // Update accessibility expanded state.
      this.tagLine.setAttribute("aria-expanded", this.expanded);
    } else {
      this.elt.classList.remove("expandable");
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
    // ARIA level should already be set when the container markup is created.
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

    return [...this.children.children].filter(node => node.container)
                                      .map(node => node.container);
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
      this.htmlElt.classList.add("dragging");
      this.elt.classList.add("dragging");
      this.markup.doc.body.classList.add("dragging");
      rootElt.setAttribute("aria-dropeffect", "move");
    } else {
      this.htmlElt.classList.remove("dragging");
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
    if (isInput && keyCode !== KeyCodes.DOM_VK_TAB) {
      return;
    }

    switch (keyCode) {
      case KeyCodes.DOM_VK_TAB:
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
      case KeyCodes.DOM_VK_ESCAPE:
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
      this.markup.inspector.followAttributeLink(type, link);
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
      let container = this.markup.getContainer(this.markup._rootNode);
      if (container) {
        container.elt.setAttribute("aria-activedescendant", this.id);
      }
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
    // Prevent the html tree from expanding when an event bubble is clicked.
    if (event.target.dataset.event) {
      event.stopPropagation();
      return;
    }

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
    this.elt.removeEventListener("mousedown", this._onMouseDown);
    this.elt.removeEventListener("dblclick", this._onToggle);
    this.tagLine.removeEventListener("keydown", this._onKeyDown, true);
    if (this.win) {
      this.win.removeEventListener("mouseup", this._onMouseUp, true);
      this.win.removeEventListener("mousemove", this._onMouseMove, true);
    }

    this.win = null;
    this.htmlElt = null;

    if (this.expander) {
      this.expander.removeEventListener("click", this._onToggle);
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

module.exports = MarkupContainer;
