/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const flags = require("devtools/shared/flags");
const { ELLIPSIS } = require("devtools/shared/l10n");
const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(
  this,
  "KeyShortcuts",
  "devtools/client/shared/key-shortcuts"
);

const MAX_LABEL_LENGTH = 40;

const NS_XHTML = "http://www.w3.org/1999/xhtml";
const SCROLL_REPEAT_MS = 100;

// Some margin may be required for visible element detection.
const SCROLL_MARGIN = 1;

const SHADOW_ROOT_TAGNAME = "#shadow-root";

/**
 * Component to replicate functionality of XUL arrowscrollbox
 * for breadcrumbs
 *
 * @param {Window} win The window containing the breadcrumbs
 * @parem {DOMNode} container The element in which to put the scroll box
 */
function ArrowScrollBox(win, container) {
  this.win = win;
  this.doc = win.document;
  this.container = container;
  EventEmitter.decorate(this);
  this.init();
}

ArrowScrollBox.prototype = {
  // Scroll behavior, exposed for testing
  scrollBehavior: "smooth",

  /**
   * Build the HTML, add to the DOM and start listening to
   * events
   */
  init: function() {
    this.constructHtml();

    this.onScroll = this.onScroll.bind(this);
    this.onStartBtnClick = this.onStartBtnClick.bind(this);
    this.onEndBtnClick = this.onEndBtnClick.bind(this);
    this.onStartBtnDblClick = this.onStartBtnDblClick.bind(this);
    this.onEndBtnDblClick = this.onEndBtnDblClick.bind(this);
    this.onUnderflow = this.onUnderflow.bind(this);
    this.onOverflow = this.onOverflow.bind(this);

    this.inner.addEventListener("scroll", this.onScroll);
    this.startBtn.addEventListener("mousedown", this.onStartBtnClick);
    this.endBtn.addEventListener("mousedown", this.onEndBtnClick);
    this.startBtn.addEventListener("dblclick", this.onStartBtnDblClick);
    this.endBtn.addEventListener("dblclick", this.onEndBtnDblClick);

    // Overflow and underflow are moz specific events
    this.inner.addEventListener("underflow", this.onUnderflow);
    this.inner.addEventListener("overflow", this.onOverflow);
  },

  /**
   * Scroll to the specified element using the current scroll behavior
   * @param {Element} element element to scroll
   * @param {String} block desired alignment of element after scrolling
   */
  scrollToElement: function(element, block) {
    element.scrollIntoView({ block: block, behavior: this.scrollBehavior });
  },

  /**
   * Call the given function once; then continuously
   * while the mouse button is held
   * @param {Function} repeatFn the function to repeat while the button is held
   */
  clickOrHold: function(repeatFn) {
    let timer;
    const container = this.container;

    function handleClick() {
      cancelHold();
      repeatFn();
    }

    const window = this.win;
    function cancelHold() {
      window.clearTimeout(timer);
      container.removeEventListener("mouseout", cancelHold);
      container.removeEventListener("mouseup", handleClick);
    }

    function repeated() {
      repeatFn();
      timer = window.setTimeout(repeated, SCROLL_REPEAT_MS);
    }

    container.addEventListener("mouseout", cancelHold);
    container.addEventListener("mouseup", handleClick);
    timer = window.setTimeout(repeated, SCROLL_REPEAT_MS);
  },

  /**
   * When start button is dbl clicked scroll to first element
   */
  onStartBtnDblClick: function() {
    const children = this.inner.childNodes;
    if (children.length < 1) {
      return;
    }

    const element = this.inner.childNodes[0];
    this.scrollToElement(element, "start");
  },

  /**
   * When end button is dbl clicked scroll to last element
   */
  onEndBtnDblClick: function() {
    const children = this.inner.childNodes;
    if (children.length < 1) {
      return;
    }

    const element = children[children.length - 1];
    this.scrollToElement(element, "start");
  },

  /**
   * When start arrow button is clicked scroll towards first element
   */
  onStartBtnClick: function() {
    const scrollToStart = () => {
      const element = this.getFirstInvisibleElement();
      if (!element) {
        return;
      }

      this.scrollToElement(element, "start");
    };

    this.clickOrHold(scrollToStart);
  },

  /**
   * When end arrow button is clicked scroll towards last element
   */
  onEndBtnClick: function() {
    const scrollToEnd = () => {
      const element = this.getLastInvisibleElement();
      if (!element) {
        return;
      }

      this.scrollToElement(element, "end");
    };

    this.clickOrHold(scrollToEnd);
  },

  /**
   * Event handler for scrolling, update the
   * enabled/disabled status of the arrow buttons
   */
  onScroll: function() {
    const first = this.getFirstInvisibleElement();
    if (!first) {
      this.startBtn.setAttribute("disabled", "true");
    } else {
      this.startBtn.removeAttribute("disabled");
    }

    const last = this.getLastInvisibleElement();
    if (!last) {
      this.endBtn.setAttribute("disabled", "true");
    } else {
      this.endBtn.removeAttribute("disabled");
    }
  },

  /**
   * On underflow, make the arrow buttons invisible
   */
  onUnderflow: function() {
    this.startBtn.style.visibility = "collapse";
    this.endBtn.style.visibility = "collapse";
    this.emit("underflow");
  },

  /**
   * On overflow, show the arrow buttons
   */
  onOverflow: function() {
    this.startBtn.style.visibility = "visible";
    this.endBtn.style.visibility = "visible";
    this.emit("overflow");
  },

  /**
   * Check whether the element is to the left of its container but does
   * not also span the entire container.
   * @param {Number} left the left scroll point of the container
   * @param {Number} right the right edge of the container
   * @param {Number} elementLeft the left edge of the element
   * @param {Number} elementRight the right edge of the element
   */
  elementLeftOfContainer: function(left, right, elementLeft, elementRight) {
    return (
      elementLeft < left - SCROLL_MARGIN && elementRight < right - SCROLL_MARGIN
    );
  },

  /**
   * Check whether the element is to the right of its container but does
   * not also span the entire container.
   * @param {Number} left the left scroll point of the container
   * @param {Number} right the right edge of the container
   * @param {Number} elementLeft the left edge of the element
   * @param {Number} elementRight the right edge of the element
   */
  elementRightOfContainer: function(left, right, elementLeft, elementRight) {
    return (
      elementLeft > left + SCROLL_MARGIN && elementRight > right + SCROLL_MARGIN
    );
  },

  /**
   * Get the first (i.e. furthest left for LTR)
   * non or partly visible element in the scroll box
   */
  getFirstInvisibleElement: function() {
    const elementsList = Array.from(this.inner.childNodes).reverse();

    const predicate = this.elementLeftOfContainer;
    return this.findFirstWithBounds(elementsList, predicate);
  },

  /**
   * Get the last (i.e. furthest right for LTR)
   * non or partly visible element in the scroll box
   */
  getLastInvisibleElement: function() {
    const predicate = this.elementRightOfContainer;
    return this.findFirstWithBounds(this.inner.childNodes, predicate);
  },

  /**
   * Find the first element that matches the given predicate, called with bounds
   * information
   * @param {Array} elements an ordered list of elements
   * @param {Function} predicate a function to be called with bounds
   * information
   */
  findFirstWithBounds: function(elements, predicate) {
    const left = this.inner.scrollLeft;
    const right = left + this.inner.clientWidth;
    for (const element of elements) {
      const elementLeft = element.offsetLeft - element.parentElement.offsetLeft;
      const elementRight = elementLeft + element.offsetWidth;

      // Check that the starting edge of the element is out of the visible area
      // and that the ending edge does not span the whole container
      if (predicate(left, right, elementLeft, elementRight)) {
        return element;
      }
    }

    return null;
  },

  /**
   * Build the HTML for the scroll box and insert it into the DOM
   */
  constructHtml: function() {
    this.startBtn = this.createElement(
      "div",
      "scrollbutton-up",
      this.container
    );
    this.createElement("div", "toolbarbutton-icon", this.startBtn);

    this.createElement(
      "div",
      "arrowscrollbox-overflow-start-indicator",
      this.container
    );
    this.inner = this.createElement(
      "div",
      "html-arrowscrollbox-inner",
      this.container
    );
    this.createElement(
      "div",
      "arrowscrollbox-overflow-end-indicator",
      this.container
    );

    this.endBtn = this.createElement(
      "div",
      "scrollbutton-down",
      this.container
    );
    this.createElement("div", "toolbarbutton-icon", this.endBtn);
  },

  /**
   * Create an XHTML element with the given class name, and append it to the
   * parent.
   * @param {String} tagName name of the tag to create
   * @param {String} className class of the element
   * @param {DOMNode} parent the parent node to which it should be appended
   * @return {DOMNode} The new element
   */
  createElement: function(tagName, className, parent) {
    const el = this.doc.createElementNS(NS_XHTML, tagName);
    el.className = className;
    if (parent) {
      parent.appendChild(el);
    }

    return el;
  },

  /**
   * Remove event handlers and clean up
   */
  destroy: function() {
    this.inner.removeEventListener("scroll", this.onScroll);
    this.startBtn.removeEventListener("mousedown", this.onStartBtnClick);
    this.endBtn.removeEventListener("mousedown", this.onEndBtnClick);
    this.startBtn.removeEventListener("dblclick", this.onStartBtnDblClick);
    this.endBtn.removeEventListener("dblclick", this.onRightBtnDblClick);

    // Overflow and underflow are moz specific events
    this.inner.removeEventListener("underflow", this.onUnderflow);
    this.inner.removeEventListener("overflow", this.onOverflow);
  },
};

/**
 * Display the ancestors of the current node and its children.
 * Only one "branch" of children are displayed (only one line).
 *
 * Mechanism:
 * - If no nodes displayed yet:
 *   then display the ancestor of the selected node and the selected node;
 *   else select the node;
 * - If the selected node is the last node displayed, append its first (if any).
 *
 * @param {InspectorPanel} inspector The inspector hosting this widget.
 */
function HTMLBreadcrumbs(inspector) {
  this.inspector = inspector;
  this.selection = this.inspector.selection;
  this.win = this.inspector.panelWin;
  this.doc = this.inspector.panelDoc;
  this._init();
}

exports.HTMLBreadcrumbs = HTMLBreadcrumbs;

HTMLBreadcrumbs.prototype = {
  get walker() {
    return this.inspector.walker;
  },

  _init: function() {
    this.outer = this.doc.getElementById("inspector-breadcrumbs");
    this.arrowScrollBox = new ArrowScrollBox(this.win, this.outer);

    this.container = this.arrowScrollBox.inner;
    this.scroll = this.scroll.bind(this);
    this.arrowScrollBox.on("overflow", this.scroll);

    this.outer.addEventListener("click", this, true);
    this.outer.addEventListener("mouseover", this, true);
    this.outer.addEventListener("mouseout", this, true);
    this.outer.addEventListener("focus", this, true);

    this.handleShortcut = this.handleShortcut.bind(this);

    if (flags.testing) {
      // In tests, we start listening immediately to avoid having to simulate a focus.
      this.initKeyShortcuts();
    } else {
      this.outer.addEventListener(
        "focus",
        () => {
          this.initKeyShortcuts();
        },
        { once: true }
      );
    }

    // We will save a list of already displayed nodes in this array.
    this.nodeHierarchy = [];

    // Last selected node in nodeHierarchy.
    this.currentIndex = -1;

    // Used to build a unique breadcrumb button Id.
    this.breadcrumbsWidgetItemId = 0;

    this.update = this.update.bind(this);
    this.updateWithMutations = this.updateWithMutations.bind(this);
    this.updateSelectors = this.updateSelectors.bind(this);
    this.selection.on("new-node-front", this.update);
    this.selection.on("pseudoclass", this.updateSelectors);
    this.selection.on("attribute-changed", this.updateSelectors);
    this.inspector.on("markupmutation", this.updateWithMutations);
    this.update();
  },

  initKeyShortcuts() {
    this.shortcuts = new KeyShortcuts({ window: this.win, target: this.outer });
    this.shortcuts.on("Right", this.handleShortcut);
    this.shortcuts.on("Left", this.handleShortcut);
  },

  /**
   * Build a string that represents the node: tagName#id.class1.class2.
   * @param {NodeFront} node The node to pretty-print
   * @return {String}
   */
  prettyPrintNodeAsText: function(node) {
    let text = node.isShadowRoot ? SHADOW_ROOT_TAGNAME : node.displayName;
    if (node.isMarkerPseudoElement) {
      text = "::marker";
    } else if (node.isBeforePseudoElement) {
      text = "::before";
    } else if (node.isAfterPseudoElement) {
      text = "::after";
    }

    if (node.id) {
      text += "#" + node.id;
    }

    if (node.className) {
      const classList = node.className.split(/\s+/);
      for (let i = 0; i < classList.length; i++) {
        text += "." + classList[i];
      }
    }

    for (const pseudo of node.pseudoClassLocks) {
      text += pseudo;
    }

    return text;
  },

  /**
   * Build <span>s that represent the node:
   *   <span class="breadcrumbs-widget-item-tag">tagName</span>
   *   <span class="breadcrumbs-widget-item-id">#id</span>
   *   <span class="breadcrumbs-widget-item-classes">.class1.class2</span>
   * @param {NodeFront} node The node to pretty-print
   * @returns {DocumentFragment}
   */
  prettyPrintNodeAsXHTML: function(node) {
    const tagLabel = this.doc.createElementNS(NS_XHTML, "span");
    tagLabel.className = "breadcrumbs-widget-item-tag plain";

    const idLabel = this.doc.createElementNS(NS_XHTML, "span");
    idLabel.className = "breadcrumbs-widget-item-id plain";

    const classesLabel = this.doc.createElementNS(NS_XHTML, "span");
    classesLabel.className = "breadcrumbs-widget-item-classes plain";

    const pseudosLabel = this.doc.createElementNS(NS_XHTML, "span");
    pseudosLabel.className = "breadcrumbs-widget-item-pseudo-classes plain";

    let tagText = node.isShadowRoot ? SHADOW_ROOT_TAGNAME : node.displayName;
    if (node.isMarkerPseudoElement) {
      tagText = "::marker";
    } else if (node.isBeforePseudoElement) {
      tagText = "::before";
    } else if (node.isAfterPseudoElement) {
      tagText = "::after";
    }
    let idText = node.id ? "#" + node.id : "";
    let classesText = "";

    if (node.className) {
      const classList = node.className.split(/\s+/);
      for (let i = 0; i < classList.length; i++) {
        classesText += "." + classList[i];
      }
    }

    // Figure out which element (if any) needs ellipsing.
    // Substring for that element, then clear out any extras
    // (except for pseudo elements).
    const maxTagLength = MAX_LABEL_LENGTH;
    const maxIdLength = MAX_LABEL_LENGTH - tagText.length;
    const maxClassLength = MAX_LABEL_LENGTH - tagText.length - idText.length;

    if (tagText.length > maxTagLength) {
      tagText = tagText.substr(0, maxTagLength) + ELLIPSIS;
      idText = classesText = "";
    } else if (idText.length > maxIdLength) {
      idText = idText.substr(0, maxIdLength) + ELLIPSIS;
      classesText = "";
    } else if (classesText.length > maxClassLength) {
      classesText = classesText.substr(0, maxClassLength) + ELLIPSIS;
    }

    tagLabel.textContent = tagText;
    idLabel.textContent = idText;
    classesLabel.textContent = classesText;
    pseudosLabel.textContent = node.pseudoClassLocks.join("");

    const fragment = this.doc.createDocumentFragment();
    fragment.appendChild(tagLabel);
    fragment.appendChild(idLabel);
    fragment.appendChild(classesLabel);
    fragment.appendChild(pseudosLabel);

    return fragment;
  },

  /**
   * Generic event handler.
   * @param {DOMEvent} event.
   */
  handleEvent: function(event) {
    if (event.type == "click" && event.button == 0) {
      this.handleClick(event);
    } else if (event.type == "mouseover") {
      this.handleMouseOver(event);
    } else if (event.type == "mouseout") {
      this.handleMouseOut(event);
    } else if (event.type == "focus") {
      this.handleFocus(event);
    }
  },

  /**
   * Focus event handler. When breadcrumbs container gets focus,
   * aria-activedescendant needs to be updated to currently selected
   * breadcrumb. Ensures that the focus stays on the container at all times.
   * @param {DOMEvent} event.
   */
  handleFocus: function(event) {
    event.stopPropagation();

    const node = this.nodeHierarchy[this.currentIndex];
    if (node) {
      this.outer.setAttribute("aria-activedescendant", node.button.id);
    } else {
      this.outer.removeAttribute("aria-activedescendant");
    }

    this.outer.focus();
  },

  /**
   * On click navigate to the correct node.
   * @param {DOMEvent} event.
   */
  handleClick: function(event) {
    const target = event.originalTarget;
    if (target.tagName == "button") {
      target.onBreadcrumbsClick();
    }
  },

  /**
   * On mouse over, highlight the corresponding content DOM Node.
   * @param {DOMEvent} event.
   */
  handleMouseOver: function(event) {
    const target = event.originalTarget;
    if (target.tagName == "button") {
      target.onBreadcrumbsHover();
    }
  },

  /**
   * On mouse out, make sure to unhighlight.
   * @param {DOMEvent} event.
   */
  handleMouseOut: function(event) {
    this.inspector.highlighters.hideHighlighterType(
      this.inspector.highlighters.TYPES.BOXMODEL
    );
  },

  /**
   * Handle a keyboard shortcut supported by the breadcrumbs widget.
   *
   * @param {String} name
   *        Name of the keyboard shortcut received.
   * @param {DOMEvent} event
   *        Original event that triggered the shortcut.
   */
  handleShortcut: function(event) {
    if (!this.selection.isElementNode()) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();

    this.keyPromise = (this.keyPromise || Promise.resolve(null)).then(() => {
      let currentnode;

      const isLeft = event.code === "ArrowLeft";
      const isRight = event.code === "ArrowRight";

      if (isLeft && this.currentIndex != 0) {
        currentnode = this.nodeHierarchy[this.currentIndex - 1];
      } else if (isRight && this.currentIndex < this.nodeHierarchy.length - 1) {
        currentnode = this.nodeHierarchy[this.currentIndex + 1];
      } else {
        return null;
      }

      this.outer.setAttribute("aria-activedescendant", currentnode.button.id);
      return this.selection.setNodeFront(currentnode.node, {
        reason: "breadcrumbs",
      });
    });
  },

  /**
   * Remove nodes and clean up.
   */
  destroy: function() {
    this.selection.off("new-node-front", this.update);
    this.selection.off("pseudoclass", this.updateSelectors);
    this.selection.off("attribute-changed", this.updateSelectors);
    this.inspector.off("markupmutation", this.updateWithMutations);

    this.container.removeEventListener("click", this, true);
    this.container.removeEventListener("mouseover", this, true);
    this.container.removeEventListener("mouseout", this, true);
    this.container.removeEventListener("focus", this, true);

    if (this.shortcuts) {
      this.shortcuts.destroy();
    }

    this.empty();

    this.arrowScrollBox.off("overflow", this.scroll);
    this.arrowScrollBox.destroy();
    this.arrowScrollBox = null;
    this.outer = null;
    this.container = null;
    this.nodeHierarchy = null;

    this.isDestroyed = true;
  },

  /**
   * Empty the breadcrumbs container.
   */
  empty: function() {
    while (this.container.hasChildNodes()) {
      this.container.firstChild.remove();
    }
  },

  /**
   * Set which button represent the selected node.
   * @param {Number} index Index of the displayed-button to select.
   */
  setCursor: function(index) {
    // Unselect the previously selected button
    if (
      this.currentIndex > -1 &&
      this.currentIndex < this.nodeHierarchy.length
    ) {
      this.nodeHierarchy[this.currentIndex].button.removeAttribute("checked");
    }
    if (index > -1) {
      this.nodeHierarchy[index].button.setAttribute("checked", "true");
    } else {
      // Unset active active descendant when all buttons are unselected.
      this.outer.removeAttribute("aria-activedescendant");
    }
    this.currentIndex = index;
  },

  /**
   * Get the index of the node in the cache.
   * @param {NodeFront} node.
   * @returns {Number} The index for this node or -1 if not found.
   */
  indexOf: function(node) {
    for (let i = this.nodeHierarchy.length - 1; i >= 0; i--) {
      if (this.nodeHierarchy[i].node === node) {
        return i;
      }
    }
    return -1;
  },

  /**
   * Remove all the buttons and their references in the cache after a given
   * index.
   * @param {Number} index.
   */
  cutAfter: function(index) {
    while (this.nodeHierarchy.length > index + 1) {
      const toRemove = this.nodeHierarchy.pop();
      this.container.removeChild(toRemove.button);
    }
  },

  /**
   * Build a button representing the node.
   * @param {NodeFront} node The node from the page.
   * @return {DOMNode} The <button> for this node.
   */
  buildButton: function(node) {
    const button = this.doc.createElementNS(NS_XHTML, "button");
    button.appendChild(this.prettyPrintNodeAsXHTML(node));
    button.className = "breadcrumbs-widget-item";
    button.id = "breadcrumbs-widget-item-" + this.breadcrumbsWidgetItemId++;

    button.setAttribute("tabindex", "-1");
    button.setAttribute("title", this.prettyPrintNodeAsText(node));

    button.onclick = () => {
      button.focus();
    };

    button.onBreadcrumbsClick = () => {
      this.selection.setNodeFront(node, { reason: "breadcrumbs" });
    };

    button.onBreadcrumbsHover = () => {
      this.inspector.highlighters.showHighlighterTypeForNode(
        this.inspector.highlighters.TYPES.BOXMODEL,
        node
      );
    };

    return button;
  },

  /**
   * Connecting the end of the breadcrumbs to a node.
   * @param {NodeFront} node The node to reach.
   */
  expand: function(node) {
    const fragment = this.doc.createDocumentFragment();
    let lastButtonInserted = null;
    const originalLength = this.nodeHierarchy.length;
    let stopNode = null;
    if (originalLength > 0) {
      stopNode = this.nodeHierarchy[originalLength - 1].node;
    }
    while (node && node != stopNode) {
      if (node.tagName || node.isShadowRoot) {
        const button = this.buildButton(node);
        fragment.insertBefore(button, lastButtonInserted);
        lastButtonInserted = button;
        this.nodeHierarchy.splice(originalLength, 0, {
          node,
          button,
          currentPrettyPrintText: this.prettyPrintNodeAsText(node),
        });
      }
      node = node.parentOrHost();
    }
    this.container.appendChild(fragment, this.container.firstChild);
  },

  /**
   * Find the "youngest" ancestor of a node which is already in the breadcrumbs.
   * @param {NodeFront} node.
   * @return {Number} Index of the ancestor in the cache, or -1 if not found.
   */
  getCommonAncestor: function(node) {
    while (node) {
      const idx = this.indexOf(node);
      if (idx > -1) {
        return idx;
      }
      node = node.parentNode();
    }
    return -1;
  },

  /**
   * Ensure the selected node is visible.
   */
  scroll: function() {
    // FIXME bug 684352: make sure its immediate neighbors are visible too.
    if (!this.isDestroyed) {
      const element = this.nodeHierarchy[this.currentIndex].button;
      this.arrowScrollBox.scrollToElement(element, "end");
    }
  },

  /**
   * Update all button outputs.
   */
  updateSelectors: function() {
    if (this.isDestroyed) {
      return;
    }

    for (let i = this.nodeHierarchy.length - 1; i >= 0; i--) {
      const { node, button, currentPrettyPrintText } = this.nodeHierarchy[i];

      // If the output of the node doesn't change, skip the update.
      const textOutput = this.prettyPrintNodeAsText(node);
      if (currentPrettyPrintText === textOutput) {
        continue;
      }

      // Otherwise, update the whole markup for the button.
      while (button.hasChildNodes()) {
        button.firstChild.remove();
      }
      button.appendChild(this.prettyPrintNodeAsXHTML(node));
      button.setAttribute("title", textOutput);

      this.nodeHierarchy[i].currentPrettyPrintText = textOutput;
    }
  },

  /**
   * Given a list of mutation changes (passed by the markupmutation event),
   * decide whether or not they are "interesting" to the current state of the
   * breadcrumbs widget, i.e. at least one of them should cause part of the
   * widget to be updated.
   * @param {Array} mutations The mutations array.
   * @return {Boolean}
   */
  _hasInterestingMutations: function(mutations) {
    if (!mutations || !mutations.length) {
      return false;
    }

    for (const { type, added, removed, target, attributeName } of mutations) {
      if (type === "childList") {
        // Only interested in childList mutations if the added or removed
        // nodes are currently displayed.
        return (
          added.some(node => this.indexOf(node) > -1) ||
          removed.some(node => this.indexOf(node) > -1)
        );
      } else if (type === "attributes" && this.indexOf(target) > -1) {
        // Only interested in attributes mutations if the target is
        // currently displayed, and the attribute is either id or class.
        return attributeName === "class" || attributeName === "id";
      }
    }

    // Catch all return in case the mutations array was empty, or in case none
    // of the changes iterated above were interesting.
    return false;
  },

  /**
   * Update the breadcrumbs display when a new node is selected and there are
   * mutations.
   * @param {Array} mutations An array of mutations in case this was called as
   * the "markupmutation" event listener.
   */
  updateWithMutations(mutations) {
    return this.update("markupmutation", mutations);
  },

  /**
   * Update the breadcrumbs display when a new node is selected.
   * @param {String} reason The reason for the update, if any.
   * @param {Array} mutations An array of mutations in case this was called as
   * the "markupmutation" event listener.
   */
  update: function(reason, mutations) {
    if (this.isDestroyed) {
      return;
    }

    const hasInterestingMutations = this._hasInterestingMutations(mutations);
    if (reason === "markupmutation" && !hasInterestingMutations) {
      return;
    }

    if (!this.selection.isConnected()) {
      // remove all the crumbs
      this.cutAfter(-1);
      return;
    }

    // If this was an interesting deletion; then trim the breadcrumb trail
    let trimmed = false;
    if (reason === "markupmutation") {
      for (const { type, removed } of mutations) {
        if (type !== "childList") {
          continue;
        }

        for (const node of removed) {
          const removedIndex = this.indexOf(node);
          if (removedIndex > -1) {
            this.cutAfter(removedIndex - 1);
            trimmed = true;
          }
        }
      }
    }

    if (!this.selection.isElementNode() && !this.selection.isShadowRootNode()) {
      // no selection
      this.setCursor(-1);
      if (trimmed) {
        // Since something changed, notify the interested parties.
        this.inspector.emit("breadcrumbs-updated", this.selection.nodeFront);
      }
      return;
    }

    let idx = this.indexOf(this.selection.nodeFront);

    // Is the node already displayed in the breadcrumbs?
    // (and there are no mutations that need re-display of the crumbs)
    if (idx > -1 && !hasInterestingMutations) {
      // Yes. We select it.
      this.setCursor(idx);
    } else {
      // No. Is the breadcrumbs display empty?
      if (this.nodeHierarchy.length > 0) {
        // No. We drop all the element that are not direct ancestors
        // of the selection
        const parent = this.selection.nodeFront.parentNode();
        const ancestorIdx = this.getCommonAncestor(parent);
        this.cutAfter(ancestorIdx);
      }
      // we append the missing button between the end of the breadcrumbs display
      // and the current node.
      this.expand(this.selection.nodeFront);

      // we select the current node button
      idx = this.indexOf(this.selection.nodeFront);
      this.setCursor(idx);
    }

    const doneUpdating = this.inspector.updating("breadcrumbs");

    this.updateSelectors();

    // Make sure the selected node and its neighbours are visible.
    setTimeout(() => {
      try {
        this.scroll();
        this.inspector.emit("breadcrumbs-updated", this.selection.nodeFront);
        doneUpdating();
      } catch (e) {
        // Only log this as an error if we haven't been destroyed in the meantime.
        if (!this.isDestroyed) {
          console.error(e);
        }
      }
    }, 0);
  },
};
