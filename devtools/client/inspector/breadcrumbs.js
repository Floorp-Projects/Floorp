/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Ci} = require("chrome");
const Services = require("Services");
const promise = require("promise");
const FocusManager = Services.focus;
const {waitForTick} = require("devtools/shared/DevToolsUtils");

const ELLIPSIS = Services.prefs.getComplexValue(
    "intl.ellipsis",
    Ci.nsIPrefLocalizedString).data;
const MAX_LABEL_LENGTH = 40;

const NS_XHTML = "http://www.w3.org/1999/xhtml";
const SCROLL_REPEAT_MS = 100;

loader.lazyRequireGetter(this, "EventEmitter",
                         "devtools/shared/event-emitter");

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

  /**
   * Build the HTML, add to the DOM and start listening to
   * events
   */
  init: function () {
    this.constructHtml();

    this.onUnderflow();

    this.onScroll = this.onScroll.bind(this);
    this.onStartBtnClick = this.onStartBtnClick.bind(this);
    this.onEndBtnClick = this.onEndBtnClick.bind(this);
    this.onStartBtnDblClick = this.onStartBtnDblClick.bind(this);
    this.onEndBtnDblClick = this.onEndBtnDblClick.bind(this);
    this.onUnderflow = this.onUnderflow.bind(this);
    this.onOverflow = this.onOverflow.bind(this);

    this.inner.addEventListener("scroll", this.onScroll, false);
    this.startBtn.addEventListener("mousedown", this.onStartBtnClick, false);
    this.endBtn.addEventListener("mousedown", this.onEndBtnClick, false);
    this.startBtn.addEventListener("dblclick", this.onStartBtnDblClick, false);
    this.endBtn.addEventListener("dblclick", this.onEndBtnDblClick, false);

    // Overflow and underflow are moz specific events
    this.inner.addEventListener("underflow", this.onUnderflow, false);
    this.inner.addEventListener("overflow", this.onOverflow, false);
  },

  /**
   * Call the given function once; then continuously
   * while the mouse button is held
   * @param {repeatFn} the function to repeat while the button is held
   */
  clickOrHold: function (repeatFn) {
    let timer;
    let container = this.container;

    function handleClick() {
      cancelHold();
      repeatFn();
    }

    let window = this.win;
    function cancelHold() {
      window.clearTimeout(timer);
      container.removeEventListener("mouseout", cancelHold, false);
      container.removeEventListener("mouseup", handleClick, false);
    }

    function repeated() {
      repeatFn();
      timer = window.setTimeout(repeated, SCROLL_REPEAT_MS);
    }

    container.addEventListener("mouseout", cancelHold, false);
    container.addEventListener("mouseup", handleClick, false);
    timer = window.setTimeout(repeated, SCROLL_REPEAT_MS);
  },

  /**
   * When start button is dbl clicked scroll to first element
   */
  onStartBtnDblClick: function () {
    let children = this.inner.childNodes;
    if (children.length < 1) {
      return;
    }

    let element = this.inner.childNodes[0];
    element.scrollIntoView({ block: "start", behavior: "smooth" });
  },

  /**
   * When end button is dbl clicked scroll to last element
   */
  onEndBtnDblClick: function () {
    let children = this.inner.childNodes;
    if (children.length < 1) {
      return;
    }

    let element = children[children.length - 1];
    element.scrollIntoView({ block: "start", behavior: "smooth" });
  },

  /**
   * When start arrow button is clicked scroll towards first element
   */
  onStartBtnClick: function () {
    let scrollToStart = () => {
      let element = this.getFirstInvisibleElement();
      if (!element) {
        return;
      }

      element.scrollIntoView({ block: "start", behavior: "smooth" });
    };

    this.clickOrHold(scrollToStart);
  },

  /**
   * When end arrow button is clicked scroll towards last element
   */
  onEndBtnClick: function () {
    let scrollToEnd = () => {
      let element = this.getLastInvisibleElement();
      if (!element) {
        return;
      }

      element.scrollIntoView({ block: "end", behavior: "smooth" });
    };

    this.clickOrHold(scrollToEnd);
  },

  /**
   * Event handler for scrolling, update the
   * enabled/disabled status of the arrow buttons
   */
  onScroll: function () {
    let first = this.getFirstInvisibleElement();
    if (!first) {
      this.startBtn.setAttribute("disabled", "true");
    } else {
      this.startBtn.removeAttribute("disabled");
    }

    let last = this.getLastInvisibleElement();
    if (!last) {
      this.endBtn.setAttribute("disabled", "true");
    } else {
      this.endBtn.removeAttribute("disabled");
    }
  },

  /**
   * On underflow, make the arrow buttons invisible
   */
  onUnderflow: function () {
    this.startBtn.style.visibility = "collapse";
    this.endBtn.style.visibility = "collapse";
    this.emit("underflow");
  },

  /**
   * On overflow, show the arrow buttons
   */
  onOverflow: function () {
    this.startBtn.style.visibility = "visible";
    this.endBtn.style.visibility = "visible";
    this.emit("overflow");
  },

  /**
   * Get the first (i.e. furthest left for LTR)
   * non visible element in the scroll box
   */
  getFirstInvisibleElement: function () {
    let start = this.inner.scrollLeft;
    let end = this.inner.scrollLeft + this.inner.clientWidth;
    let crumbs = this.inner.childNodes;
    for (let i = crumbs.length - 1; i > -1; i--) {
      let element = crumbs[i];
      let elementRight = element.offsetLeft + element.offsetWidth;
      if (element.offsetLeft < start) {
        // edge case, check the element isn't already visible
        if (elementRight >= end) {
          continue;
        }
        return element;
      }
    }

    return null;
  },

  /**
   * Get the last (i.e. furthest right for LTR)
   * non-visible element in the scroll box
   */
  getLastInvisibleElement: function () {
    let end = this.inner.scrollLeft + this.inner.clientWidth;
    let elementStart = 0;
    for (let element of this.inner.childNodes) {
      let elementEnd = elementStart + element.offsetWidth;
      if (elementEnd > end) {
        // Edge case: check the element isn't bigger than the
        // container and thus already in view
        if (elementStart > this.inner.scrollLeft) {
          return element;
        }
      }

      elementStart = elementEnd;
    }

    return null;
  },

  /**
   * Build the HTML for the scroll box and insert it into the DOM
   */
  constructHtml: function () {
    this.startBtn = this.createElement("div", "scrollbutton-up",
                                       this.container);
    this.createElement("div", "toolbarbutton-icon", this.startBtn);

    this.createElement("div", "arrowscrollbox-overflow-start-indicator",
                       this.container);
    this.inner = this.createElement("div", "html-arrowscrollbox-inner",
                                    this.container);
    this.createElement("div", "arrowscrollbox-overflow-end-indicator",
                       this.container);

    this.endBtn = this.createElement("div", "scrollbutton-down",
                                     this.container);
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
  createElement: function (tagName, className, parent) {
    let el = this.doc.createElementNS(NS_XHTML, tagName);
    el.className = className;
    if (parent) {
      parent.appendChild(el);
    }

    return el;
  },

  /**
   * Remove event handlers and clean up
   */
  destroy: function () {
    this.inner.removeEventListener("scroll", this.onScroll, false);
    this.startBtn.removeEventListener("mousedown",
                                      this.onStartBtnClick, false);
    this.endBtn.removeEventListener("mousedown", this.onEndBtnClick, false);
    this.startBtn.removeEventListener("dblclick",
                                      this.onStartBtnDblClick, false);
    this.endBtn.removeEventListener("dblclick",
                                    this.onRightBtnDblClick, false);

    // Overflow and underflow are moz specific events
    this.inner.removeEventListener("underflow", this.onUnderflow, false);
    this.inner.removeEventListener("overflow", this.onOverflow, false);
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
  this.chromeWin = this.inspector.panelWin;
  this.chromeDoc = this.inspector.panelDoc;
  this._init();
}

exports.HTMLBreadcrumbs = HTMLBreadcrumbs;

HTMLBreadcrumbs.prototype = {
  get walker() {
    return this.inspector.walker;
  },

  _init: function () {
    this.outer = this.chromeDoc.getElementById("inspector-breadcrumbs");
    this.arrowScrollBox = new ArrowScrollBox(
        this.chromeWin,
        this.outer);

    this.container = this.arrowScrollBox.inner;
    this.arrowScrollBox.on("overflow", this.scroll);

    // These separators are used for CSS purposes only, and are positioned
    // off screen, but displayed with -moz-element.
    this.separators = this.chromeDoc.createElementNS(NS_XHTML, "div");
    this.separators.className = "breadcrumb-separator-container";
    this.separators.innerHTML =
                      "<div id='breadcrumb-separator-before'></div>" +
                      "<div id='breadcrumb-separator-after'></div>" +
                      "<div id='breadcrumb-separator-normal'></div>";
    this.container.parentNode.appendChild(this.separators);

    this.outer.addEventListener("click", this, true);
    this.outer.addEventListener("keypress", this, true);
    this.outer.addEventListener("mouseover", this, true);
    this.outer.addEventListener("mouseleave", this, true);
    this.outer.addEventListener("focus", this, true);

    // We will save a list of already displayed nodes in this array.
    this.nodeHierarchy = [];

    // Last selected node in nodeHierarchy.
    this.currentIndex = -1;

    this.update = this.update.bind(this);
    this.updateSelectors = this.updateSelectors.bind(this);
    this.selection.on("new-node-front", this.update);
    this.selection.on("pseudoclass", this.updateSelectors);
    this.selection.on("attribute-changed", this.updateSelectors);
    this.inspector.on("markupmutation", this.update);
    this.update();
  },

  /**

   * Build a string that represents the node: tagName#id.class1.class2.
   * @param {NodeFront} node The node to pretty-print
   * @return {String}
   */
  prettyPrintNodeAsText: function (node) {
    let text = node.displayName;
    if (node.isPseudoElement) {
      text = node.isBeforePseudoElement ? "::before" : "::after";
    }

    if (node.id) {
      text += "#" + node.id;
    }

    if (node.className) {
      let classList = node.className.split(/\s+/);
      for (let i = 0; i < classList.length; i++) {
        text += "." + classList[i];
      }
    }

    for (let pseudo of node.pseudoClassLocks) {
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
  prettyPrintNodeAsXHTML: function (node) {
    let tagLabel = this.chromeDoc.createElementNS(NS_XHTML, "span");
    tagLabel.className = "breadcrumbs-widget-item-tag plain";

    let idLabel = this.chromeDoc.createElementNS(NS_XHTML, "span");
    idLabel.className = "breadcrumbs-widget-item-id plain";

    let classesLabel = this.chromeDoc.createElementNS(NS_XHTML, "span");
    classesLabel.className = "breadcrumbs-widget-item-classes plain";

    let pseudosLabel = this.chromeDoc.createElementNS(NS_XHTML, "span");
    pseudosLabel.className = "breadcrumbs-widget-item-pseudo-classes plain";

    let tagText = node.displayName;
    if (node.isPseudoElement) {
      tagText = node.isBeforePseudoElement ? "::before" : "::after";
    }
    let idText = node.id ? ("#" + node.id) : "";
    let classesText = "";

    if (node.className) {
      let classList = node.className.split(/\s+/);
      for (let i = 0; i < classList.length; i++) {
        classesText += "." + classList[i];
      }
    }

    // Figure out which element (if any) needs ellipsing.
    // Substring for that element, then clear out any extras
    // (except for pseudo elements).
    let maxTagLength = MAX_LABEL_LENGTH;
    let maxIdLength = MAX_LABEL_LENGTH - tagText.length;
    let maxClassLength = MAX_LABEL_LENGTH - tagText.length - idText.length;

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

    let fragment = this.chromeDoc.createDocumentFragment();
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
  handleEvent: function (event) {
    if (event.type == "click" && event.button == 0) {
      this.handleClick(event);
    } else if (event.type == "keypress" && this.selection.isElementNode()) {
      this.handleKeyPress(event);
    } else if (event.type == "mouseover") {
      this.handleMouseOver(event);
    } else if (event.type == "mouseleave") {
      this.handleMouseLeave(event);
    } else if (event.type == "focus") {
      this.handleFocus(event);
    }
  },

  /**
   * Focus event handler. When breadcrumbs container gets focus, if there is an
   * already selected breadcrumb, move focus to it.
   * @param {DOMEvent} event.
   */
  handleFocus: function (event) {
    let control = this.container.querySelector(
      ".breadcrumbs-widget-item[checked]");
    if (!this.suspendFocus && control && control !== event.target) {
      // If we already have a selected breadcrumb and focus target is not it,
      // move focus to selected breadcrumb
      event.preventDefault();
      control.focus();
    }
    this.suspendFocus = false;
  },

  /**
   * On click navigate to the correct node.
   * @param {DOMEvent} event.
   */
  handleClick: function (event) {
    // When clicking a button temporarily suspend the behaviour that refocuses
    // the currently selected button, to prevent flicking back to that button
    // See Bug 1272011
    this.suspendFocus = true;
    let target = event.originalTarget;
    if (target.tagName == "button") {
      target.onBreadcrumbsClick();
    }
  },

  /**
   * On mouse over, highlight the corresponding content DOM Node.
   * @param {DOMEvent} event.
   */
  handleMouseOver: function (event) {
    let target = event.originalTarget;
    if (target.tagName == "button") {
      target.onBreadcrumbsHover();
    }
  },

  /**
   * On mouse leave, make sure to unhighlight.
   * @param {DOMEvent} event.
   */
  handleMouseLeave: function (event) {
    this.inspector.toolbox.highlighterUtils.unhighlight();
  },

  /**
   * On keypress, navigate through the list of breadcrumbs with the left/right
   * arrow keys.
   * @param {DOMEvent} event.
   */
  handleKeyPress: function (event) {
    let win = this.chromeWin;
    let {keyCode, shiftKey, metaKey, ctrlKey, altKey} = event;

    // Only handle left, right, tab and shift tab, let anything else bubble up
    // so native shortcuts work.
    let hasModifier = metaKey || ctrlKey || altKey || shiftKey;
    let isLeft = keyCode === win.KeyEvent.DOM_VK_LEFT && !hasModifier;
    let isRight = keyCode === win.KeyEvent.DOM_VK_RIGHT && !hasModifier;
    let isTab = keyCode === win.KeyEvent.DOM_VK_TAB && !hasModifier;
    let isShiftTab = keyCode === win.KeyEvent.DOM_VK_TAB && shiftKey &&
                     !metaKey && !ctrlKey && !altKey;

    if (!isLeft && !isRight && !isTab && !isShiftTab) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();

    this.keyPromise = (this.keyPromise || promise.resolve(null)).then(() => {
      if (isLeft && this.currentIndex != 0) {
        let node = this.nodeHierarchy[this.currentIndex - 1].node;
        return this.selection.setNodeFront(node, "breadcrumbs");
      } else if (isRight && this.currentIndex < this.nodeHierarchy.length - 1) {
        let node = this.nodeHierarchy[this.currentIndex + 1].node;
        return this.selection.setNodeFront(node, "breadcrumbs");
      } else if (isTab || isShiftTab) {
        // Tabbing when breadcrumbs or its contents are focused should move
        // focus to next/previous focusable element relative to breadcrumbs
        // themselves.
        let elm, type;
        if (shiftKey) {
          elm = this.container;
          type = FocusManager.MOVEFOCUS_BACKWARD;
        } else {
          // To move focus to next element following the breadcrumbs, relative
          // element needs to be the last element in breadcrumbs' subtree.
          let last = this.container.lastChild;
          while (last && last.lastChild) {
            last = last.lastChild;
          }
          elm = last;
          type = FocusManager.MOVEFOCUS_FORWARD;
        }
        FocusManager.moveFocus(win, elm, type, 0);
      }

      return null;
    });
  },

  /**
   * Remove nodes and clean up.
   */
  destroy: function () {
    this.selection.off("new-node-front", this.update);
    this.selection.off("pseudoclass", this.updateSelectors);
    this.selection.off("attribute-changed", this.updateSelectors);
    this.inspector.off("markupmutation", this.update);

    this.container.removeEventListener("click", this, true);
    this.container.removeEventListener("keypress", this, true);
    this.container.removeEventListener("mouseover", this, true);
    this.container.removeEventListener("mouseleave", this, true);
    this.container.removeEventListener("focus", this, true);

    this.empty();
    this.separators.remove();

    this.arrowScrollBox.off("overflow", this.scroll);
    this.arrowScrollBox.destroy();
    this.arrowScrollBox = null;
    this.outer = null;
    this.container = null;
    this.separators = null;
    this.nodeHierarchy = null;

    this.isDestroyed = true;
  },

  /**
   * Empty the breadcrumbs container.
   */
  empty: function () {
    while (this.container.hasChildNodes()) {
      this.container.firstChild.remove();
    }
  },

  /**
   * Set which button represent the selected node.
   * @param {Number} index Index of the displayed-button to select.
   */
  setCursor: function (index) {
    // Unselect the previously selected button
    if (this.currentIndex > -1
        && this.currentIndex < this.nodeHierarchy.length) {
      this.nodeHierarchy[this.currentIndex].button.removeAttribute("checked");
    }
    if (index > -1) {
      this.nodeHierarchy[index].button.setAttribute("checked", "true");
      if (this.hadFocus) {
        this.nodeHierarchy[index].button.focus();
      }
    }
    this.currentIndex = index;
  },

  /**
   * Get the index of the node in the cache.
   * @param {NodeFront} node.
   * @returns {Number} The index for this node or -1 if not found.
   */
  indexOf: function (node) {
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
  cutAfter: function (index) {
    while (this.nodeHierarchy.length > (index + 1)) {
      let toRemove = this.nodeHierarchy.pop();
      this.container.removeChild(toRemove.button);
    }
  },

  /**
   * Build a button representing the node.
   * @param {NodeFront} node The node from the page.
   * @return {DOMNode} The <button> for this node.
   */
  buildButton: function (node) {
    let button = this.chromeDoc.createElementNS(NS_XHTML, "button");
    button.appendChild(this.prettyPrintNodeAsXHTML(node));
    button.className = "breadcrumbs-widget-item";

    button.setAttribute("title", this.prettyPrintNodeAsText(node));

    button.onkeypress = function onBreadcrumbsKeypress(e) {
      if (e.charCode == Ci.nsIDOMKeyEvent.DOM_VK_SPACE ||
          e.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN) {
        button.click();
      }
    };

    button.onclick = () => {
      button.focus();
    };

    button.onBreadcrumbsClick = () => {
      this.selection.setNodeFront(node, "breadcrumbs");
    };

    button.onBreadcrumbsHover = () => {
      this.inspector.toolbox.highlighterUtils.highlightNodeFront(node);
    };

    return button;
  },

  /**
   * Connecting the end of the breadcrumbs to a node.
   * @param {NodeFront} node The node to reach.
   */
  expand: function (node) {
    let fragment = this.chromeDoc.createDocumentFragment();
    let lastButtonInserted = null;
    let originalLength = this.nodeHierarchy.length;
    let stopNode = null;
    if (originalLength > 0) {
      stopNode = this.nodeHierarchy[originalLength - 1].node;
    }
    while (node && node != stopNode) {
      if (node.tagName) {
        let button = this.buildButton(node);
        fragment.insertBefore(button, lastButtonInserted);
        lastButtonInserted = button;
        this.nodeHierarchy.splice(originalLength, 0, {
          node,
          button,
          currentPrettyPrintText: this.prettyPrintNodeAsText(node)
        });
      }
      node = node.parentNode();
    }
    this.container.appendChild(fragment, this.container.firstChild);
  },

  /**
   * Find the "youngest" ancestor of a node which is already in the breadcrumbs.
   * @param {NodeFront} node.
   * @return {Number} Index of the ancestor in the cache, or -1 if not found.
   */
  getCommonAncestor: function (node) {
    while (node) {
      let idx = this.indexOf(node);
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
  scroll: function () {
    // FIXME bug 684352: make sure its immediate neighbors are visible too.
    let element = this.nodeHierarchy[this.currentIndex].button;
    element.scrollIntoView({ block: "end", behavior: "smooth" });
  },

  /**
   * Update all button outputs.
   */
  updateSelectors: function () {
    if (this.isDestroyed) {
      return;
    }

    for (let i = this.nodeHierarchy.length - 1; i >= 0; i--) {
      let {node, button, currentPrettyPrintText} = this.nodeHierarchy[i];

      // If the output of the node doesn't change, skip the update.
      let textOutput = this.prettyPrintNodeAsText(node);
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
  _hasInterestingMutations: function (mutations) {
    if (!mutations || !mutations.length) {
      return false;
    }

    for (let {type, added, removed, target, attributeName} of mutations) {
      if (type === "childList") {
        // Only interested in childList mutations if the added or removed
        // nodes are currently displayed.
        return added.some(node => this.indexOf(node) > -1) ||
               removed.some(node => this.indexOf(node) > -1);
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
   * Update the breadcrumbs display when a new node is selected.
   * @param {String} reason The reason for the update, if any.
   * @param {Array} mutations An array of mutations in case this was called as
   * the "markupmutation" event listener.
   */
  update: function (reason, mutations) {
    if (this.isDestroyed) {
      return;
    }

    if (reason !== "markupmutation") {
      this.inspector.hideNodeMenu();
    }

    let hasInterestingMutations = this._hasInterestingMutations(mutations);
    if (reason === "markupmutation" && !hasInterestingMutations) {
      return;
    }

    let cmdDispatcher = this.chromeDoc.commandDispatcher;
    this.hadFocus = (cmdDispatcher.focusedElement &&
                     cmdDispatcher.focusedElement.parentNode == this.container);

    if (!this.selection.isConnected()) {
      // remove all the crumbs
      this.cutAfter(-1);
      return;
    }

    if (!this.selection.isElementNode()) {
      // no selection
      this.setCursor(-1);
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
        let parent = this.selection.nodeFront.parentNode();
        let ancestorIdx = this.getCommonAncestor(parent);
        this.cutAfter(ancestorIdx);
      }
      // we append the missing button between the end of the breadcrumbs display
      // and the current node.
      this.expand(this.selection.nodeFront);

      // we select the current node button
      idx = this.indexOf(this.selection.nodeFront);
      this.setCursor(idx);
    }

    let doneUpdating = this.inspector.updating("breadcrumbs");

    this.updateSelectors();

    // Make sure the selected node and its neighbours are visible.
    waitForTick().then(() => {
      this.scroll();
      this.inspector.emit("breadcrumbs-updated", this.selection.nodeFront);
      doneUpdating();
    });
  }
};
