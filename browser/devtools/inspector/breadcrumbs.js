/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Cu, Ci} = require("chrome");

const PSEUDO_CLASSES = [":hover", ":active", ":focus"];
const ENSURE_SELECTION_VISIBLE_DELAY = 50; // ms

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/DOMHelpers.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");

const LOW_PRIORITY_ELEMENTS = {
  "HEAD": true,
  "BASE": true,
  "BASEFONT": true,
  "ISINDEX": true,
  "LINK": true,
  "META": true,
  "SCRIPT": true,
  "STYLE": true,
  "TITLE": true,
};

///////////////////////////////////////////////////////////////////////////
//// HTML Breadcrumbs

/**
 * Display the ancestors of the current node and its children.
 * Only one "branch" of children are displayed (only one line).
 *
 * FIXME: Bug 822388 - Use the BreadcrumbsWidget in the Inspector.
 *
 * Mechanism:
 * . If no nodes displayed yet:
 *    then display the ancestor of the selected node and the selected node;
 *   else select the node;
 * . If the selected node is the last node displayed, append its first (if any).
 */
function HTMLBreadcrumbs(aInspector)
{
  this.inspector = aInspector;
  this.selection = this.inspector.selection;
  this.chromeWin = this.inspector.panelWin;
  this.chromeDoc = this.inspector.panelDoc;
  this.DOMHelpers = new DOMHelpers(this.chromeWin);
  this._init();
}

exports.HTMLBreadcrumbs = HTMLBreadcrumbs;

HTMLBreadcrumbs.prototype = {
  _init: function BC__init()
  {
    this.container = this.chromeDoc.getElementById("inspector-breadcrumbs");
    this.container.addEventListener("mousedown", this, true);
    this.container.addEventListener("keypress", this, true);

    // We will save a list of already displayed nodes in this array.
    this.nodeHierarchy = [];

    // Last selected node in nodeHierarchy.
    this.currentIndex = -1;

    // By default, hide the arrows. We let the <scrollbox> show them
    // in case of overflow.
    this.container.removeAttribute("overflows");
    this.container._scrollButtonUp.collapsed = true;
    this.container._scrollButtonDown.collapsed = true;

    this.onscrollboxreflow = function() {
      if (this.container._scrollButtonDown.collapsed)
        this.container.removeAttribute("overflows");
      else
        this.container.setAttribute("overflows", true);
    }.bind(this);

    this.container.addEventListener("underflow", this.onscrollboxreflow, false);
    this.container.addEventListener("overflow", this.onscrollboxreflow, false);

    this.update = this.update.bind(this);
    this.updateSelectors = this.updateSelectors.bind(this);
    this.selection.on("new-node", this.update);
    this.selection.on("pseudoclass", this.updateSelectors);
    this.selection.on("attribute-changed", this.updateSelectors);
    this.update();
  },

  /**
   * Build a string that represents the node: tagName#id.class1.class2.
   *
   * @param aNode The node to pretty-print
   * @returns a string
   */
  prettyPrintNodeAsText: function BC_prettyPrintNodeText(aNode)
  {
    let text = aNode.tagName.toLowerCase();
    if (aNode.id) {
      text += "#" + aNode.id;
    }
    for (let i = 0; i < aNode.classList.length; i++) {
      text += "." + aNode.classList[i];
    }
    for (let i = 0; i < PSEUDO_CLASSES.length; i++) {
      let pseudo = PSEUDO_CLASSES[i];
      if (DOMUtils.hasPseudoClassLock(aNode, pseudo)) {
        text += pseudo;
      }
    }

    return text;
  },


  /**
   * Build <label>s that represent the node:
   *   <label class="breadcrumbs-widget-item-tag">tagName</label>
   *   <label class="breadcrumbs-widget-item-id">#id</label>
   *   <label class="breadcrumbs-widget-item-classes">.class1.class2</label>
   *
   * @param aNode The node to pretty-print
   * @returns a document fragment.
   */
  prettyPrintNodeAsXUL: function BC_prettyPrintNodeXUL(aNode)
  {
    let fragment = this.chromeDoc.createDocumentFragment();

    let tagLabel = this.chromeDoc.createElement("label");
    tagLabel.className = "breadcrumbs-widget-item-tag plain";

    let idLabel = this.chromeDoc.createElement("label");
    idLabel.className = "breadcrumbs-widget-item-id plain";

    let classesLabel = this.chromeDoc.createElement("label");
    classesLabel.className = "breadcrumbs-widget-item-classes plain";

    let pseudosLabel = this.chromeDoc.createElement("label");
    pseudosLabel.className = "breadcrumbs-widget-item-pseudo-classes plain";

    tagLabel.textContent = aNode.tagName.toLowerCase();
    idLabel.textContent = aNode.id ? ("#" + aNode.id) : "";

    let classesText = "";
    for (let i = 0; i < aNode.classList.length; i++) {
      classesText += "." + aNode.classList[i];
    }
    classesLabel.textContent = classesText;

    let pseudos = PSEUDO_CLASSES.filter(function(pseudo) {
      return DOMUtils.hasPseudoClassLock(aNode, pseudo);
    }, this);
    pseudosLabel.textContent = pseudos.join("");

    fragment.appendChild(tagLabel);
    fragment.appendChild(idLabel);
    fragment.appendChild(classesLabel);
    fragment.appendChild(pseudosLabel);

    return fragment;
  },

  /**
   * Open the sibling menu.
   *
   * @param aButton the button representing the node.
   * @param aNode the node we want the siblings from.
   */
  openSiblingMenu: function BC_openSiblingMenu(aButton, aNode)
  {
    // We make sure that the targeted node is selected
    // because we want to use the nodemenu that only works
    // for inspector.selection
    this.selection.setNode(aNode, "breadcrumbs");

    let title = this.chromeDoc.createElement("menuitem");
    title.setAttribute("label", this.inspector.strings.GetStringFromName("breadcrumbs.siblings"));
    title.setAttribute("disabled", "true");

    let separator = this.chromeDoc.createElement("menuseparator");

    let items = [title, separator];

    let nodes = aNode.parentNode.childNodes;
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].nodeType == aNode.ELEMENT_NODE) {
        let item = this.chromeDoc.createElement("menuitem");
        if (nodes[i] === aNode) {
          item.setAttribute("disabled", "true");
          item.setAttribute("checked", "true");
        }

        item.setAttribute("type", "radio");
        item.setAttribute("label", this.prettyPrintNodeAsText(nodes[i]));

        let selection = this.selection;
        item.onmouseup = (function(aNode) {
          return function() {
            selection.setNode(aNode, "breadcrumbs");
          }
        })(nodes[i]);

        items.push(item);
      }
    }
    this.inspector.showNodeMenu(aButton, "before_start", items);
  },

  /**
   * Generic event handler.
   *
   * @param nsIDOMEvent event
   *        The DOM event object.
   */
  handleEvent: function BC_handleEvent(event)
  {
    if (event.type == "mousedown" && event.button == 0) {
      // on Click and Hold, open the Siblings menu

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
    }

    if (event.type == "keypress" && this.selection.isElementNode()) {
      let node = null;
      switch (event.keyCode) {
        case this.chromeWin.KeyEvent.DOM_VK_LEFT:
          if (this.currentIndex != 0) {
            node = this.nodeHierarchy[this.currentIndex - 1].node;
          }
          break;
        case this.chromeWin.KeyEvent.DOM_VK_RIGHT:
          if (this.currentIndex < this.nodeHierarchy.length - 1) {
            node = this.nodeHierarchy[this.currentIndex + 1].node;
          }
          break;
        case this.chromeWin.KeyEvent.DOM_VK_UP:
          node = this.selection.node.previousSibling;
          while (node && (node.nodeType != node.ELEMENT_NODE)) {
            node = node.previousSibling;
          }
          break;
        case this.chromeWin.KeyEvent.DOM_VK_DOWN:
          node = this.selection.node.nextSibling;
          while (node && (node.nodeType != node.ELEMENT_NODE)) {
            node = node.nextSibling;
          }
          break;
      }
      if (node) {
        this.selection.setNode(node, "breadcrumbs");
      }
      event.preventDefault();
      event.stopPropagation();
    }
  },

  /**
   * Remove nodes and delete properties.
   */
  destroy: function BC_destroy()
  {
    this.nodeHierarchy.forEach(function(crumb) {
      if (LayoutHelpers.isNodeConnected(crumb.node)) {
        DOMUtils.clearPseudoClassLocks(crumb.node);
      }
    });

    this.selection.off("new-node", this.update);
    this.selection.off("pseudoclass", this.updateSelectors);
    this.selection.off("attribute-changed", this.updateSelectors);

    this.container.removeEventListener("underflow", this.onscrollboxreflow, false);
    this.container.removeEventListener("overflow", this.onscrollboxreflow, false);
    this.onscrollboxreflow = null;

    this.empty();
    this.container.removeEventListener("mousedown", this, true);
    this.container.removeEventListener("keypress", this, true);
    this.container = null;
    this.nodeHierarchy = null;
  },

  /**
   * Empty the breadcrumbs container.
   */
  empty: function BC_empty()
  {
    while (this.container.hasChildNodes()) {
      this.container.removeChild(this.container.firstChild);
    }
  },

  /**
   * Re-init the cache and remove all the buttons.
   */
  invalidateHierarchy: function BC_invalidateHierarchy()
  {
    this.inspector.hideNodeMenu();
    this.nodeHierarchy = [];
    this.empty();
  },

  /**
   * Set which button represent the selected node.
   *
   * @param aIdx Index of the displayed-button to select
   */
  setCursor: function BC_setCursor(aIdx)
  {
    // Unselect the previously selected button
    if (this.currentIndex > -1 && this.currentIndex < this.nodeHierarchy.length) {
      this.nodeHierarchy[this.currentIndex].button.removeAttribute("checked");
    }
    if (aIdx > -1) {
      this.nodeHierarchy[aIdx].button.setAttribute("checked", "true");
      if (this.hadFocus)
        this.nodeHierarchy[aIdx].button.focus();
    }
    this.currentIndex = aIdx;
  },

  /**
   * Get the index of the node in the cache.
   *
   * @param aNode
   * @returns integer the index, -1 if not found
   */
  indexOf: function BC_indexOf(aNode)
  {
    let i = this.nodeHierarchy.length - 1;
    for (let i = this.nodeHierarchy.length - 1; i >= 0; i--) {
      if (this.nodeHierarchy[i].node === aNode) {
        return i;
      }
    }
    return -1;
  },

  /**
   * Remove all the buttons and their references in the cache
   * after a given index.
   *
   * @param aIdx
   */
  cutAfter: function BC_cutAfter(aIdx)
  {
    while (this.nodeHierarchy.length > (aIdx + 1)) {
      let toRemove = this.nodeHierarchy.pop();
      this.container.removeChild(toRemove.button);
    }
  },

  /**
   * Build a button representing the node.
   *
   * @param aNode The node from the page.
   * @returns aNode The <button>.
   */
  buildButton: function BC_buildButton(aNode)
  {
    let button = this.chromeDoc.createElement("button");
    button.appendChild(this.prettyPrintNodeAsXUL(aNode));
    button.className = "breadcrumbs-widget-item";

    button.setAttribute("tooltiptext", this.prettyPrintNodeAsText(aNode));

    button.onkeypress = function onBreadcrumbsKeypress(e) {
      if (e.charCode == Ci.nsIDOMKeyEvent.DOM_VK_SPACE ||
          e.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN)
        button.click();
    }

    button.onBreadcrumbsClick = function onBreadcrumbsClick() {
      this.selection.setNode(aNode, "breadcrumbs");
    }.bind(this);

    button.onclick = (function _onBreadcrumbsRightClick(event) {
      button.focus();
      if (event.button == 2) {
        this.openSiblingMenu(button, aNode);
      }
    }).bind(this);

    button.onBreadcrumbsHold = (function _onBreadcrumbsHold() {
      this.openSiblingMenu(button, aNode);
    }).bind(this);
    return button;
  },

  /**
   * Connecting the end of the breadcrumbs to a node.
   *
   * @param aNode The node to reach.
   */
  expand: function BC_expand(aNode)
  {
      let fragment = this.chromeDoc.createDocumentFragment();
      let toAppend = aNode;
      let lastButtonInserted = null;
      let originalLength = this.nodeHierarchy.length;
      let stopNode = null;
      if (originalLength > 0) {
        stopNode = this.nodeHierarchy[originalLength - 1].node;
      }
      while (toAppend && toAppend.tagName && toAppend != stopNode) {
        let button = this.buildButton(toAppend);
        fragment.insertBefore(button, lastButtonInserted);
        lastButtonInserted = button;
        this.nodeHierarchy.splice(originalLength, 0, {node: toAppend, button: button});
        toAppend = this.DOMHelpers.getParentObject(toAppend);
      }
      this.container.appendChild(fragment, this.container.firstChild);
  },

  /**
   * Get a child of a node that can be displayed in the breadcrumbs
   * and that is probably visible. See LOW_PRIORITY_ELEMENTS.
   *
   * @param aNode The parent node.
   * @returns nsIDOMNode|null
   */
  getInterestingFirstNode: function BC_getInterestingFirstNode(aNode)
  {
    let nextChild = this.DOMHelpers.getChildObject(aNode, 0);
    let fallback = null;

    while (nextChild) {
      if (nextChild.nodeType == aNode.ELEMENT_NODE) {
        if (!(nextChild.tagName in LOW_PRIORITY_ELEMENTS)) {
          return nextChild;
        }
        if (!fallback) {
          fallback = nextChild;
        }
      }
      nextChild = this.DOMHelpers.getNextSibling(nextChild);
    }
    return fallback;
  },


  /**
   * Find the "youngest" ancestor of a node which is already in the breadcrumbs.
   *
   * @param aNode
   * @returns Index of the ancestor in the cache
   */
  getCommonAncestor: function BC_getCommonAncestor(aNode)
  {
    let node = aNode;
    while (node) {
      let idx = this.indexOf(node);
      if (idx > -1) {
        return idx;
      } else {
        node = this.DOMHelpers.getParentObject(node);
      }
    }
    return -1;
  },

  /**
   * Make sure that the latest node in the breadcrumbs is not the selected node
   * if the selected node still has children.
   */
  ensureFirstChild: function BC_ensureFirstChild()
  {
    // If the last displayed node is the selected node
    if (this.currentIndex == this.nodeHierarchy.length - 1) {
      let node = this.nodeHierarchy[this.currentIndex].node;
      let child = this.getInterestingFirstNode(node);
      // If the node has a child
      if (child) {
        // Show this child
        this.expand(child);
      }
    }
  },

  /**
   * Ensure the selected node is visible.
   */
  scroll: function BC_scroll()
  {
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

  updateSelectors: function BC_updateSelectors()
  {
    for (let i = this.nodeHierarchy.length - 1; i >= 0; i--) {
      let crumb = this.nodeHierarchy[i];
      let button = crumb.button;

      while(button.hasChildNodes()) {
        button.removeChild(button.firstChild);
      }
      button.appendChild(this.prettyPrintNodeAsXUL(crumb.node));
      button.setAttribute("tooltiptext", this.prettyPrintNodeAsText(crumb.node));
    }
  },

  /**
   * Update the breadcrumbs display when a new node is selected.
   */
  update: function BC_update()
  {
    this.inspector.hideNodeMenu();

    let cmdDispatcher = this.chromeDoc.commandDispatcher;
    this.hadFocus = (cmdDispatcher.focusedElement &&
                     cmdDispatcher.focusedElement.parentNode == this.container);

    if (!this.selection.isConnected()) {
      this.cutAfter(-1); // remove all the crumbs
      return;
    }

    if (!this.selection.isElementNode()) {
      this.setCursor(-1); // no selection
      return;
    }

    let idx = this.indexOf(this.selection.node);

    // Is the node already displayed in the breadcrumbs?
    if (idx > -1) {
      // Yes. We select it.
      this.setCursor(idx);
    } else {
      // No. Is the breadcrumbs display empty?
      if (this.nodeHierarchy.length > 0) {
        // No. We drop all the element that are not direct ancestors
        // of the selection
        let parent = this.DOMHelpers.getParentObject(this.selection.node);
        let idx = this.getCommonAncestor(parent);
        this.cutAfter(idx);
      }
      // we append the missing button between the end of the breadcrumbs display
      // and the current node.
      this.expand(this.selection.node);

      // we select the current node button
      idx = this.indexOf(this.selection.node);
      this.setCursor(idx);
    }
    // Add the first child of the very last node of the breadcrumbs if possible.
    this.ensureFirstChild();
    this.updateSelectors();

    // Make sure the selected node and its neighbours are visible.
    this.scroll();
  },
}

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});
