/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Ci} = require("chrome");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
const promise = require("promise");

const ENSURE_SELECTION_VISIBLE_DELAY = 50; // ms
const ELLIPSIS = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data;
const MAX_LABEL_LENGTH = 40;
const LOW_PRIORITY_ELEMENTS = {
  "HEAD": true,
  "BASE": true,
  "BASEFONT": true,
  "ISINDEX": true,
  "LINK": true,
  "META": true,
  "SCRIPT": true,
  "STYLE": true,
  "TITLE": true
};

/**
 * Display the ancestors of the current node and its children.
 * Only one "branch" of children are displayed (only one line).
 *
 * FIXME: Bug 822388 - Use the BreadcrumbsWidget in the Inspector.
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

  _init: function() {
    this.container = this.chromeDoc.getElementById("inspector-breadcrumbs");

    // These separators are used for CSS purposes only, and are positioned
    // off screen, but displayed with -moz-element.
    this.separators = this.chromeDoc.createElement("box");
    this.separators.className = "breadcrumb-separator-container";
    this.separators.innerHTML =
                      "<box id='breadcrumb-separator-before'></box>" +
                      "<box id='breadcrumb-separator-after'></box>" +
                      "<box id='breadcrumb-separator-normal'></box>";
    this.container.parentNode.appendChild(this.separators);

    this.container.addEventListener("mousedown", this, true);
    this.container.addEventListener("keypress", this, true);
    this.container.addEventListener("mouseover", this, true);
    this.container.addEventListener("mouseleave", this, true);

    // We will save a list of already displayed nodes in this array.
    this.nodeHierarchy = [];

    // Last selected node in nodeHierarchy.
    this.currentIndex = -1;

    // By default, hide the arrows. We let the <scrollbox> show them
    // in case of overflow.
    this.container.removeAttribute("overflows");
    this.container._scrollButtonUp.collapsed = true;
    this.container._scrollButtonDown.collapsed = true;

    this.onscrollboxreflow = () => {
      if (this.container._scrollButtonDown.collapsed) {
        this.container.removeAttribute("overflows");
      } else {
        this.container.setAttribute("overflows", true);
      }
    };

    this.container.addEventListener("underflow", this.onscrollboxreflow, false);
    this.container.addEventListener("overflow", this.onscrollboxreflow, false);

    this.update = this.update.bind(this);
    this.updateSelectors = this.updateSelectors.bind(this);
    this.selection.on("new-node-front", this.update);
    this.selection.on("pseudoclass", this.updateSelectors);
    this.selection.on("attribute-changed", this.updateSelectors);
    this.inspector.on("markupmutation", this.update);
    this.update();
  },

  /**
   * Include in a promise's then() chain to reject the chain
   * when the breadcrumbs' selection has changed while the promise
   * was outstanding.
   */
  selectionGuard: function() {
    let selection = this.selection.nodeFront;
    return result => {
      if (selection != this.selection.nodeFront) {
        return promise.reject("selection-changed");
      }
      return result;
    };
  },

  /**
   * Warn if rejection was caused by selection change, print an error otherwise.
   * @param {Error} err
   */
  selectionGuardEnd: function(err) {
    if (err === "selection-changed") {
      console.warn("Asynchronous operation was aborted as selection changed.");
    } else {
      console.error(err);
    }
  },

  /**
   * Build a string that represents the node: tagName#id.class1.class2.
   * @param {NodeFront} node The node to pretty-print
   * @return {String}
   */
  prettyPrintNodeAsText: function(node) {
    let text = node.tagName.toLowerCase();
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
   * Build <label>s that represent the node:
   *   <label class="breadcrumbs-widget-item-tag">tagName</label>
   *   <label class="breadcrumbs-widget-item-id">#id</label>
   *   <label class="breadcrumbs-widget-item-classes">.class1.class2</label>
   * @param {NodeFront} node The node to pretty-print
   * @returns {DocumentFragment}
   */
  prettyPrintNodeAsXUL: function(node) {
    let fragment = this.chromeDoc.createDocumentFragment();

    let tagLabel = this.chromeDoc.createElement("label");
    tagLabel.className = "breadcrumbs-widget-item-tag plain";

    let idLabel = this.chromeDoc.createElement("label");
    idLabel.className = "breadcrumbs-widget-item-id plain";

    let classesLabel = this.chromeDoc.createElement("label");
    classesLabel.className = "breadcrumbs-widget-item-classes plain";

    let pseudosLabel = this.chromeDoc.createElement("label");
    pseudosLabel.className = "breadcrumbs-widget-item-pseudo-classes plain";

    let tagText = node.tagName.toLowerCase();
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

    fragment.appendChild(tagLabel);
    fragment.appendChild(idLabel);
    fragment.appendChild(classesLabel);
    fragment.appendChild(pseudosLabel);

    return fragment;
  },

  /**
   * Open the sibling menu.
   * @param {DOMNode} button the button representing the node.
   * @param {NodeFront} node the node we want the siblings from.
   */
  openSiblingMenu: function(button, node) {
    // We make sure that the targeted node is selected
    // because we want to use the nodemenu that only works
    // for inspector.selection
    this.navigateTo(node);

    // Build a list of extra menu items that will be appended at the end of the
    // inspector node context menu.
    let items = [this.chromeDoc.createElement("menuseparator")];

    this.walker.siblings(node, {
      whatToShow: Ci.nsIDOMNodeFilter.SHOW_ELEMENT
    }).then(siblings => {
      let nodes = siblings.nodes;
      for (let i = 0; i < nodes.length; i++) {
        // Skip siblings of the documentElement node.
        if (nodes[i].nodeType !== Ci.nsIDOMNode.ELEMENT_NODE) {
          continue;
        }

        let item = this.chromeDoc.createElement("menuitem");
        if (nodes[i] === node) {
          item.setAttribute("disabled", "true");
          item.setAttribute("checked", "true");
        }

        item.setAttribute("type", "radio");
        item.setAttribute("label", this.prettyPrintNodeAsText(nodes[i]));

        let self = this;
        item.onmouseup = (function(node) {
          return function() {
            self.navigateTo(node);
          };
        })(nodes[i]);

        items.push(item);
      }

      // Append the items to the inspector node context menu and show the menu.
      this.inspector.showNodeMenu(button, "before_start", items);
    });
  },

  /**
   * Generic event handler.
   * @param {DOMEvent} event.
   */
  handleEvent: function(event) {
    if (event.type == "mousedown" && event.button == 0) {
      this.handleMouseDown(event);
    } else if (event.type == "keypress" && this.selection.isElementNode()) {
      this.handleKeyPress(event);
    } else if (event.type == "mouseover") {
      this.handleMouseOver(event);
    } else if (event.type == "mouseleave") {
      this.handleMouseLeave(event);
    }
  },

  /**
   * On click and hold, open the siblings menu.
   * @param {DOMEvent} event.
   */
  handleMouseDown: function(event) {
    let timer;
    let container = this.container;

    function openMenu(event) {
      cancelHold();
      let target = event.originalTarget;
      if (target.tagName == "button") {
        target.onBreadcrumbsHold();
      }
    }

    function handleClick(event) {
      cancelHold();
      let target = event.originalTarget;
      if (target.tagName == "button") {
        target.onBreadcrumbsClick();
      }
    }

    let window = this.chromeWin;
    function cancelHold(event) {
      window.clearTimeout(timer);
      container.removeEventListener("mouseout", cancelHold, false);
      container.removeEventListener("mouseup", handleClick, false);
    }

    container.addEventListener("mouseout", cancelHold, false);
    container.addEventListener("mouseup", handleClick, false);
    timer = window.setTimeout(openMenu, 500, event);
  },

  /**
   * On mouse over, highlight the corresponding content DOM Node.
   * @param {DOMEvent} event.
   */
  handleMouseOver: function(event) {
    let target = event.originalTarget;
    if (target.tagName == "button") {
      target.onBreadcrumbsHover();
    }
  },

  /**
   * On mouse leave, make sure to unhighlight.
   * @param {DOMEvent} event.
   */
  handleMouseLeave: function(event) {
    this.inspector.toolbox.highlighterUtils.unhighlight();
  },

  /**
   * On key press, navigate the node hierarchy.
   * @param {DOMEvent} event.
   */
  handleKeyPress: function(event) {
    let navigate = promise.resolve(null);

    this._keyPromise = (this._keyPromise || promise.resolve(null)).then(() => {
      switch (event.keyCode) {
        case this.chromeWin.KeyEvent.DOM_VK_LEFT:
          if (this.currentIndex != 0) {
            navigate = promise.resolve(
              this.nodeHierarchy[this.currentIndex - 1].node);
          }
          break;
        case this.chromeWin.KeyEvent.DOM_VK_RIGHT:
          if (this.currentIndex < this.nodeHierarchy.length - 1) {
            navigate = promise.resolve(
              this.nodeHierarchy[this.currentIndex + 1].node);
          }
          break;
        case this.chromeWin.KeyEvent.DOM_VK_UP:
          navigate = this.walker.previousSibling(this.selection.nodeFront, {
            whatToShow: Ci.nsIDOMNodeFilter.SHOW_ELEMENT
          });
          break;
        case this.chromeWin.KeyEvent.DOM_VK_DOWN:
          navigate = this.walker.nextSibling(this.selection.nodeFront, {
            whatToShow: Ci.nsIDOMNodeFilter.SHOW_ELEMENT
          });
          break;
      }

      return navigate.then(node => this.navigateTo(node));
    });

    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * Remove nodes and clean up.
   */
  destroy: function() {
    this.selection.off("new-node-front", this.update);
    this.selection.off("pseudoclass", this.updateSelectors);
    this.selection.off("attribute-changed", this.updateSelectors);
    this.inspector.off("markupmutation", this.update);

    this.container.removeEventListener("underflow", this.onscrollboxreflow, false);
    this.container.removeEventListener("overflow", this.onscrollboxreflow, false);
    this.container.removeEventListener("mousedown", this, true);
    this.container.removeEventListener("keypress", this, true);
    this.container.removeEventListener("mouseover", this, true);
    this.container.removeEventListener("mouseleave", this, true);

    this.empty();
    this.separators.remove();

    this.onscrollboxreflow = null;
    this.container = null;
    this.separators = null;
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
    if (this.currentIndex > -1 && this.currentIndex < this.nodeHierarchy.length) {
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
    while (this.nodeHierarchy.length > (index + 1)) {
      let toRemove = this.nodeHierarchy.pop();
      this.container.removeChild(toRemove.button);
    }
  },

  navigateTo: function(node) {
    if (node) {
      this.selection.setNodeFront(node, "breadcrumbs");
    } else {
      this.inspector.emit("breadcrumbs-navigation-cancelled");
    }
  },

  /**
   * Build a button representing the node.
   * @param {NodeFront} node The node from the page.
   * @return {DOMNode} The <button> for this node.
   */
  buildButton: function(node) {
    let button = this.chromeDoc.createElement("button");
    button.appendChild(this.prettyPrintNodeAsXUL(node));
    button.className = "breadcrumbs-widget-item";

    button.setAttribute("tooltiptext", this.prettyPrintNodeAsText(node));

    button.onkeypress = function onBreadcrumbsKeypress(e) {
      if (e.charCode == Ci.nsIDOMKeyEvent.DOM_VK_SPACE ||
          e.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN) {
        button.click();
      }
    };

    button.onBreadcrumbsClick = () => {
      this.navigateTo(node);
    };

    button.onBreadcrumbsHover = () => {
      this.inspector.toolbox.highlighterUtils.highlightNodeFront(node);
    };

    button.onclick = (function _onBreadcrumbsRightClick(event) {
      button.focus();
      if (event.button == 2) {
        this.openSiblingMenu(button, node);
      }
    }).bind(this);

    button.onBreadcrumbsHold = (function _onBreadcrumbsHold() {
      this.openSiblingMenu(button, node);
    }).bind(this);
    return button;
  },

  /**
   * Connecting the end of the breadcrumbs to a node.
   * @param {NodeFront} node The node to reach.
   */
  expand: function(node) {
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
   * Get a child of a node that can be displayed in the breadcrumbs and that is
   * probably visible. See LOW_PRIORITY_ELEMENTS.
   * @param {NodeFront} node The parent node.
   * @return {Promise} Resolves to the NodeFront.
   */
  getInterestingFirstNode: function(node) {
    let deferred = promise.defer();

    let fallback = null;
    let lastNode = null;

    let moreChildren = () => {
      this.walker.children(node, {
        start: lastNode,
        maxNodes: 10,
        whatToShow: Ci.nsIDOMNodeFilter.SHOW_ELEMENT
      }).then(this.selectionGuard()).then(response => {
        for (let node of response.nodes) {
          if (!(node.tagName in LOW_PRIORITY_ELEMENTS)) {
            deferred.resolve(node);
            return;
          }
          if (!fallback) {
            fallback = node;
          }
          lastNode = node;
        }
        if (response.hasLast) {
          deferred.resolve(fallback);
          return;
        }
        moreChildren();
      }).then(null, this.selectionGuardEnd);
    };

    moreChildren();
    return deferred.promise;
  },

  /**
   * Find the "youngest" ancestor of a node which is already in the breadcrumbs.
   * @param {NodeFront} node.
   * @return {Number} Index of the ancestor in the cache, or -1 if not found.
   */
  getCommonAncestor: function(node) {
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
   * Make sure that the latest node in the breadcrumbs is not the selected node
   * if the selected node still has children.
   * @return {Promise}
   */
  ensureFirstChild: function() {
    // If the last displayed node is the selected node
    if (this.currentIndex == this.nodeHierarchy.length - 1) {
      let node = this.nodeHierarchy[this.currentIndex].node;
      return this.getInterestingFirstNode(node).then(child => {
        // If the node has a child and we've not been destroyed in the meantime
        if (child && !this.isDestroyed) {
          // Show this child
          this.expand(child);
        }
      });
    }

    return resolveNextTick(true);
  },

  /**
   * Ensure the selected node is visible.
   */
  scroll: function() {
    // FIXME bug 684352: make sure its immediate neighbors are visible too.

    let scrollbox = this.container;
    let element = this.nodeHierarchy[this.currentIndex].button;

    // Repeated calls to ensureElementIsVisible would interfere with each other
    // and may sometimes result in incorrect scroll positions.
    this.chromeWin.clearTimeout(this._ensureVisibleTimeout);
    this._ensureVisibleTimeout = this.chromeWin.setTimeout(function() {
      scrollbox.ensureElementIsVisible(element);
    }, ENSURE_SELECTION_VISIBLE_DELAY);
  },

  /**
   * Update all button outputs.
   */
  updateSelectors: function() {
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
      button.appendChild(this.prettyPrintNodeAsXUL(node));
      button.setAttribute("tooltiptext", textOutput);

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

    for (let {type, added, removed, target, attributeName} of mutations) {
      if (type === "childList") {
        // Only interested in childList mutations if the added or removed
        // nodes are currently displayed, or if it impacts the last element in
        // the breadcrumbs.
        return added.some(node => this.indexOf(node) > -1) ||
               removed.some(node => this.indexOf(node) > -1) ||
               this.indexOf(target) === this.nodeHierarchy.length - 1;
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
  update: function(reason, mutations) {
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
        let idx = this.getCommonAncestor(parent);
        this.cutAfter(idx);
      }
      // we append the missing button between the end of the breadcrumbs display
      // and the current node.
      this.expand(this.selection.nodeFront);

      // we select the current node button
      idx = this.indexOf(this.selection.nodeFront);
      this.setCursor(idx);
    }

    let doneUpdating = this.inspector.updating("breadcrumbs");
    // Add the first child of the very last node of the breadcrumbs if possible.
    this.ensureFirstChild().then(this.selectionGuard()).then(() => {
      if (this.isDestroyed) {
        return;
      }

      this.updateSelectors();

      // Make sure the selected node and its neighbours are visible.
      this.scroll();
      return resolveNextTick().then(() => {
        this.inspector.emit("breadcrumbs-updated", this.selection.nodeFront);
        doneUpdating();
      });
    }).then(null, err => {
      doneUpdating(this.selection.nodeFront);
      this.selectionGuardEnd(err);
    });
  }
};

/**
 * Returns a promise that resolves at the next main thread tick.
 */
function resolveNextTick(value) {
  let deferred = promise.defer();
  Services.tm.mainThread.dispatch(() => {
    try {
      deferred.resolve(value);
    } catch(e) {
      deferred.reject(e);
    }
  }, Ci.nsIThread.DISPATCH_NORMAL);
  return deferred.promise;
}
