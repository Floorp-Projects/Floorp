/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { KeyCodes } = require("resource://devtools/client/shared/keycodes.js");
const {
  flashElementOn,
  flashElementOff,
} = require("resource://devtools/client/inspector/markup/utils.js");

loader.lazyRequireGetter(
  this,
  "wrapMoveFocus",
  "resource://devtools/client/shared/focus.js",
  true
);

const DRAG_DROP_MIN_INITIAL_DISTANCE = 10;
const TYPES = {
  TEXT_CONTAINER: "textcontainer",
  ELEMENT_CONTAINER: "elementcontainer",
  READ_ONLY_CONTAINER: "readonlycontainer",
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
function MarkupContainer() {}

/**
 * Unique identifier used to set markup container node id.
 * @type {Number}
 */
let markupContainerID = 0;

MarkupContainer.prototype = {
  // Get the UndoStack from the MarkupView.
  get undo() {
    // undo is a lazy getter in the MarkupView.
    return this.markup.undo;
  },

  /*
   * Initialize the MarkupContainer.  Should be called while one
   * of the other contain classes is instantiated.
   *
   * @param  {MarkupView} markupView
   *         The markup view that owns this container.
   * @param  {NodeFront} node
   *         The node to display.
   * @param  {String} type
   *         The type of container to build. One of TYPES.TEXT_CONTAINER,
   *         TYPES.ELEMENT_CONTAINER, TYPES.READ_ONLY_CONTAINER
   */
  initialize(markupView, node, type) {
    this.markup = markupView;
    this.node = node;
    this.type = type;
    this.win = this.markup._frame.contentWindow;
    this.id = "treeitem-" + markupContainerID++;
    this.htmlElt = this.win.document.documentElement;

    this.buildMarkup();

    this.elt.container = this;

    this._onMouseDown = this._onMouseDown.bind(this);
    this._onClick = this._onClick.bind(this);
    this._onToggle = this._onToggle.bind(this);
    this._onKeyDown = this._onKeyDown.bind(this);
    this._eventListenersAbortController = new this.win.AbortController();

    // Binding event listeners
    const eventConfig = { signal: this._eventListenersAbortController.signal };
    this.elt.addEventListener("mousedown", this._onMouseDown, eventConfig);
    this.elt.addEventListener("click", this._onClick, eventConfig);
    this.elt.addEventListener("dblclick", this._onToggle, eventConfig);
    if (this.expander) {
      this.expander.addEventListener("click", this._onToggle, eventConfig);
    }

    // Marking the node as shown or hidden
    this.updateIsDisplayed();

    if (node.isShadowRoot) {
      this.markup.telemetry.scalarSet(
        "devtools.shadowdom.shadow_root_displayed",
        true
      );
    }
  },

  buildMarkup() {
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

    this.mutationMarker = this.win.document.createElement("div");
    this.mutationMarker.classList.add("markup-tag-mutation-marker");
    this.mutationMarker.style.setProperty("--markup-level", this.level);
    this.tagLine.appendChild(this.mutationMarker);

    this.tagState = this.win.document.createElement("span");
    this.tagState.classList.add("tag-state");
    this.tagState.setAttribute("role", "presentation");
    this.tagLine.appendChild(this.tagState);

    if (this.type !== TYPES.TEXT_CONTAINER) {
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

  toString() {
    return "[MarkupContainer for " + this.node + "]";
  },

  isPreviewable() {
    if (this.node.tagName && !this.node.isPseudoElement) {
      const tagName = this.node.tagName.toLowerCase();
      const srcAttr = this.editor.getAttributeElement("src");
      const isImage = tagName === "img" && srcAttr;
      const isCanvas = tagName === "canvas";

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
  updateIsDisplayed() {
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
  clearFocus() {
    if (!this.canFocus) {
      return;
    }

    this.canFocus = false;
    const doc = this.markup.doc;

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

  updateExpander() {
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
  setChildrenRole() {
    this.children.setAttribute(
      "role",
      this.hasChildren ? "group" : "presentation"
    );
  },

  /**
   * Set an appropriate DOM tree depth level for a node and its subtree.
   */
  updateLevel() {
    // ARIA level should already be set when the container markup is created.
    const currentLevel = this.tagLine.getAttribute("aria-level");
    const newLevel = this.level;
    if (currentLevel === newLevel) {
      // If level did not change, ignore this node and its subtree.
      return;
    }

    this.tagLine.setAttribute("aria-level", newLevel);
    const childContainers = this.getChildContainers();
    if (childContainers) {
      childContainers.forEach(container => container.updateLevel());
    }
  },

  /**
   * If the node has children, return the list of containers for all these
   * children.
   */
  getChildContainers() {
    if (!this.hasChildren) {
      return null;
    }

    return [...this.children.children]
      .filter(node => node.container)
      .map(node => node.container);
  },

  /**
   * True if the node has been visually expanded in the tree.
   */
  get expanded() {
    return !this.elt.classList.contains("collapsed");
  },

  setExpanded(value) {
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
      this.showCloseTagLine();

      this.elt.classList.remove("collapsed");
      this.expander.setAttribute("open", "");
      this.hovered = false;
      this.markup.emit("expanded");
    } else if (!value) {
      this.hideCloseTagLine();

      this.elt.classList.add("collapsed");
      this.expander.removeAttribute("open");
      this.markup.emit("collapsed");
    }

    if (this.showExpander) {
      this.tagLine.setAttribute("aria-expanded", this.expanded);
    }

    if (this.node.isShadowRoot) {
      this.markup.telemetry.scalarSet(
        "devtools.shadowdom.shadow_root_expanded",
        true
      );
    }
  },

  /**
   * Expanding a node means cloning its "inline" closing tag into a new
   * tag-line that the user can interact with and showing the children.
   */
  showCloseTagLine() {
    // Only element containers display a closing tag line. #document has no closing line.
    if (this.type !== TYPES.ELEMENT_CONTAINER) {
      return;
    }

    // Retrieve the closest .close node for this container.
    const closingTag = this.elt.querySelector(".close");
    if (!closingTag) {
      return;
    }

    // Create the closing tag-line element if not already created.
    if (!this.closeTagLine) {
      const line = this.markup.doc.createElement("div");
      line.classList.add("tag-line");
      // Closing tag is not important for accessibility.
      line.setAttribute("role", "presentation");

      const tagState = this.markup.doc.createElement("div");
      tagState.classList.add("tag-state");
      line.appendChild(tagState);

      line.appendChild(closingTag.cloneNode(true));

      flashElementOff(line);
      this.closeTagLine = line;
    }
    this.elt.appendChild(this.closeTagLine);
  },

  /**
   * Hide the closing tag-line element which should only be displayed when the container
   * is expanded.
   */
  hideCloseTagLine() {
    if (!this.closeTagLine) {
      return;
    }

    this.elt.removeChild(this.closeTagLine);
    this.closeTagLine = undefined;
  },

  parentContainer() {
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
    const rootElt = this.markup.getContainer(this.markup._rootNode).elt;
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
  isDraggable() {
    const tagName = this.node.tagName && this.node.tagName.toLowerCase();

    return (
      !this.node.isPseudoElement &&
      !this.node.isAnonymous &&
      !this.node.isDocumentElement &&
      tagName !== "body" &&
      tagName !== "head" &&
      this.win.getSelection().isCollapsed &&
      this.node.parentNode() &&
      this.node.parentNode().tagName !== null
    );
  },

  isSlotted() {
    return false;
  },

  _onKeyDown(event) {
    const { target, keyCode, shiftKey } = event;
    const isInput = this.markup._isInputOrTextarea(target);

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
          const next = wrapMoveFocus(
            this.focusableElms,
            target.nextSibling,
            shiftKey
          );
          if (next) {
            event.preventDefault();
            // Keep the editing state if possible.
            if (next._editable) {
              const e = this.markup.doc.createEvent("Event");
              e.initEvent(next._trigger, true, true);
              next.dispatchEvent(e);
            }
          }
        } else {
          const next = wrapMoveFocus(this.focusableElms, target, shiftKey);
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

  _onMouseDown(event) {
    const { target, button, metaKey, ctrlKey } = event;
    const isLeftClick = button === 0;
    const isMiddleClick = button === 1;
    const isMetaClick = isLeftClick && (metaKey || ctrlKey);

    // The "show more nodes" button already has its onclick, so early return.
    if (target.nodeName === "button") {
      return;
    }

    // Bail out when clicking on arrow expanders to avoid selecting the row.
    if (target.classList.contains("expander")) {
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

    // Middle clicks will trigger the scroll lock feature to turn on.
    // The toolbox is normally responsible for calling preventDefault when
    // needed, but we prevent markup-view mousedown events from bubbling up (via
    // stopPropagation). So we have to preventDefault here as well in order to
    // avoid this issue.
    if (isMiddleClick) {
      event.preventDefault();
    }

    // Follow attribute links if middle or meta click.
    if (isMiddleClick || isMetaClick) {
      this._openAttributeLink(target.dataset.type, target.dataset.link);
      return;
    }

    // Start node drag & drop (if the mouse moved, see _onMouseMove).
    if (isLeftClick && this.isDraggable()) {
      this._isPreDragging = true;
      this._dragStartY = event.pageY;
      this.markup._draggedContainer = this;
    }
  },

  _onClick(event) {
    const { target } = event;
    if (target.nodeName !== "button") {
      return;
    }

    // We only care about handling click/keyboard activation for buttons inside
    // "link" attributes (e.g. the "select node" button)
    const closestLinkEl = target.closest("[data-link]");
    if (!closestLinkEl) {
      return;
    }

    this._openAttributeLink(
      closestLinkEl.dataset.type,
      closestLinkEl.dataset.link
    );
    event.stopPropagation();
  },

  /**
   * Open a "link" found in a node's attribute in the markup-view
   *
   * @param {String} type: A node-attribute-parser.js ATTRIBUTE_TYPES
   * @param {String} link: A "link" as returned by the `parseAttribute` function from
   *                 node-attribute-parser.js . This can be an actual URL, but could be
   *                 something else (e.g. an element id).
   */
  _openAttributeLink(type, link) {
    // Make container tabbable descendants not tabbable (by default).
    this.canFocus = false;
    this.markup.followAttributeLink(type, link);
  },

  /**
   * On mouse up, stop dragging.
   * This handler is called from the markup view, to reduce number of listeners.
   */
  async onMouseUp() {
    this._isPreDragging = false;
    this.markup._draggedContainer = null;

    if (this.isDragging) {
      this.cancelDragging();

      if (!this.markup.dropTargetNodes) {
        return;
      }

      const { nextSibling, parent } = this.markup.dropTargetNodes;
      const { walkerFront } = parent;
      await walkerFront.insertBefore(this.node, parent, nextSibling);
      this.markup.emit("drop-completed");
    }
  },

  /**
   * On mouse move, move the dragged element and indicate the drop target.
   * This handler is called from the markup view, to reduce number of listeners.
   */
  onMouseMove(event) {
    // If this is the first move after mousedown, only start dragging after the
    // mouse has travelled a few pixels and then indicate the start position.
    const initialDiff = Math.abs(event.pageY - this._dragStartY);
    if (this._isPreDragging && initialDiff >= DRAG_DROP_MIN_INITIAL_DISTANCE) {
      this._isPreDragging = false;
      this.isDragging = true;

      // If this is the last child, use the closing <div.tag-line> of parent as
      // indicator.
      const position =
        this.elt.nextElementSibling ||
        this.markup.getContainer(this.node.parentNode()).closeTagLine;
      this.markup.indicateDragTarget(position);
    }

    if (this.isDragging) {
      const x = 0;
      let y = event.pageY - this.win.scrollY;

      // Ensure we keep the dragged element within the markup view.
      if (y < 0) {
        y = 0;
      } else if (y >= this.markup.doc.body.offsetHeight - this.win.scrollY) {
        y = this.markup.doc.body.offsetHeight - this.win.scrollY - 1;
      }

      const diff = y - this._dragStartY + this.win.scrollY;
      this.elt.style.top = diff + "px";

      const el = this.markup.doc.elementFromPoint(x, y);
      this.markup.indicateDropTarget(el);
    }
  },

  cancelDragging() {
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
  flashMutation() {
    if (!this.selected) {
      flashElementOn(this.tagState, {
        foregroundElt: this.editor.elt,
        backgroundClass: "theme-bg-contrast",
      });
      if (this._flashMutationTimer) {
        clearTimeout(this._flashMutationTimer);
        this._flashMutationTimer = null;
      }
      this._flashMutationTimer = setTimeout(() => {
        flashElementOff(this.tagState, {
          foregroundElt: this.editor.elt,
          backgroundClass: "theme-bg-contrast",
        });
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
        this.tagState.classList.add("tag-hover");
      }
      if (this.closeTagLine) {
        this.closeTagLine
          .querySelector(".tag-state")
          .classList.add("tag-hover");
      }
    } else {
      this.tagState.classList.remove("tag-hover");
      if (this.closeTagLine) {
        this.closeTagLine
          .querySelector(".tag-state")
          .classList.remove("tag-hover");
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
      const container = this.markup.getContainer(this.markup._rootNode);
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
  update(mutationBreakpoints) {
    if (this.node.pseudoClassLocks.length) {
      this.elt.classList.add("pseudoclass-locked");
    } else {
      this.elt.classList.remove("pseudoclass-locked");
    }

    if (mutationBreakpoints) {
      const allMutationsDisabled = Array.from(
        mutationBreakpoints.values()
      ).every(element => element === false);

      if (mutationBreakpoints.size > 0) {
        this.mutationMarker.classList.add("has-mutations");
        this.mutationMarker.classList.toggle(
          "mutation-breakpoint-disabled",
          allMutationsDisabled
        );
      } else {
        this.mutationMarker.classList.remove("has-mutations");
      }
    }

    this.updateIsDisplayed();

    if (this.editor.update) {
      this.editor.update();
    }
  },

  /**
   * Try to put keyboard focus on the current editor.
   */
  focus() {
    // Elements with tabindex of -1 are not focusable.
    const focusable = this.editor.elt.querySelector("[tabindex='0']");
    if (focusable) {
      focusable.focus();
    }
  },

  _onToggle(event) {
    event.stopPropagation();

    // Prevent the html tree from expanding when an event bubble, display or scrollable
    // node is clicked.
    if (
      event.target.dataset.event ||
      event.target.dataset.display ||
      event.target.dataset.scrollable
    ) {
      return;
    }

    this.expandContainer(event.altKey);
  },

  /**
   * Expands the markup container if it has children.
   *
   * @param  {Boolean} applyToDescendants
   *         Whether all descendants should also be expanded/collapsed
   */
  expandContainer(applyToDescendants) {
    if (this.hasChildren) {
      this.markup.setNodeExpanded(
        this.node,
        !this.expanded,
        applyToDescendants
      );
    }
  },

  /**
   * Get rid of event listeners and references, when the container is no longer
   * needed
   */
  destroy() {
    // Remove event listeners
    if (this._eventListenersAbortController) {
      this._eventListenersAbortController.abort();
    }
    this.tagLine.removeEventListener("keydown", this._onKeyDown, true);

    if (this.markup._draggedContainer === this) {
      this.markup._draggedContainer = null;
    }

    this.win = null;
    this.htmlElt = null;
    this._eventListenersAbortController = null;

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
  },
};

module.exports = MarkupContainer;
