/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const HTML_NS = "http://www.w3.org/1999/xhtml";

const EventEmitter = require("devtools/shared/event-emitter");
const {KeyCodes} = require("devtools/client/shared/keycodes");

/**
 * A tree widget with keyboard navigation and collapsable structure.
 *
 * @param {Node} node
 *        The container element for the tree widget.
 * @param {Object} options
 *        - emptyText {string}: text to display when no entries in the table.
 *        - defaultType {string}: The default type of the tree items. For ex.
 *          'js'
 *        - sorted {boolean}: Defaults to true. If true, tree items are kept in
 *          lexical order. If false, items will be kept in insertion order.
 *        - contextMenuId {string}: ID of context menu to be displayed on
 *          tree items.
 */
function TreeWidget(node, options = {}) {
  EventEmitter.decorate(this);

  this.document = node.ownerDocument;
  this.window = this.document.defaultView;
  this._parent = node;

  this.emptyText = options.emptyText || "";
  this.defaultType = options.defaultType;
  this.sorted = options.sorted !== false;
  this.contextMenuId = options.contextMenuId;

  this.setupRoot();

  this.placeholder = this.document.createElementNS(HTML_NS, "label");
  this.placeholder.className = "tree-widget-empty-text";
  this._parent.appendChild(this.placeholder);

  if (this.emptyText) {
    this.setPlaceholderText(this.emptyText);
  }
  // A map to hold all the passed attachment to each leaf in the tree.
  this.attachments = new Map();
}

TreeWidget.prototype = {

  _selectedLabel: null,
  _selectedItem: null,

  /**
   * Select any node in the tree.
   *
   * @param {array} ids
   *        An array of ids leading upto the selected item
   */
  set selectedItem(ids) {
    if (this._selectedLabel) {
      this._selectedLabel.classList.remove("theme-selected");
    }
    let currentSelected = this._selectedLabel;
    if (ids == -1) {
      this._selectedLabel = this._selectedItem = null;
      return;
    }
    if (!Array.isArray(ids)) {
      return;
    }
    this._selectedLabel = this.root.setSelectedItem(ids);
    if (!this._selectedLabel) {
      this._selectedItem = null;
    } else {
      if (currentSelected != this._selectedLabel) {
        this.ensureSelectedVisible();
      }
      this._selectedItem = ids;
      this.emit("select", this._selectedItem,
        this.attachments.get(JSON.stringify(ids)));
    }
  },

  /**
   * Gets the selected item in the tree.
   *
   * @return {array}
   *        An array of ids leading upto the selected item
   */
  get selectedItem() {
    return this._selectedItem;
  },

  /**
   * Returns if the passed array corresponds to the selected item in the tree.
   *
   * @return {array}
   *        An array of ids leading upto the requested item
   */
  isSelected: function(item) {
    if (!this._selectedItem || this._selectedItem.length != item.length) {
      return false;
    }

    for (let i = 0; i < this._selectedItem.length; i++) {
      if (this._selectedItem[i] != item[i]) {
        return false;
      }
    }

    return true;
  },

  destroy: function() {
    this.root.remove();
    this.root = null;
  },

  /**
   * Sets up the root container of the TreeWidget.
   */
  setupRoot: function() {
    this.root = new TreeItem(this.document);
    if (this.contextMenuId) {
      this.root.children.addEventListener("contextmenu", (event) => {
        let menu = this.document.getElementById(this.contextMenuId);
        menu.openPopupAtScreen(event.screenX, event.screenY, true);
      });
    }

    this._parent.appendChild(this.root.children);

    this.root.children.addEventListener("mousedown", e => this.onClick(e));
    this.root.children.addEventListener("keydown", e => this.onKeydown(e));
  },

  /**
   * Sets the text to be shown when no node is present in the tree
   */
  setPlaceholderText: function(text) {
    this.placeholder.textContent = text;
  },

  /**
   * Select any node in the tree.
   *
   * @param {array} id
   *        An array of ids leading upto the selected item
   */
  selectItem: function(id) {
    this.selectedItem = id;
  },

  /**
   * Selects the next visible item in the tree.
   */
  selectNextItem: function() {
    let next = this.getNextVisibleItem();
    if (next) {
      this.selectedItem = next;
    }
  },

  /**
   * Selects the previos visible item in the tree
   */
  selectPreviousItem: function() {
    let prev = this.getPreviousVisibleItem();
    if (prev) {
      this.selectedItem = prev;
    }
  },

  /**
   * Returns the next visible item in the tree
   */
  getNextVisibleItem: function() {
    let node = this._selectedLabel;
    if (node.hasAttribute("expanded") && node.nextSibling.firstChild) {
      return JSON.parse(node.nextSibling.firstChild.getAttribute("data-id"));
    }
    node = node.parentNode;
    if (node.nextSibling) {
      return JSON.parse(node.nextSibling.getAttribute("data-id"));
    }
    node = node.parentNode;
    while (node.parentNode && node != this.root.children) {
      if (node.parentNode && node.parentNode.nextSibling) {
        return JSON.parse(node.parentNode.nextSibling.getAttribute("data-id"));
      }
      node = node.parentNode;
    }
    return null;
  },

  /**
   * Returns the previous visible item in the tree
   */
  getPreviousVisibleItem: function() {
    let node = this._selectedLabel.parentNode;
    if (node.previousSibling) {
      node = node.previousSibling.firstChild;
      while (node.hasAttribute("expanded") && !node.hasAttribute("empty")) {
        if (!node.nextSibling.lastChild) {
          break;
        }
        node = node.nextSibling.lastChild.firstChild;
      }
      return JSON.parse(node.parentNode.getAttribute("data-id"));
    }
    node = node.parentNode;
    if (node.parentNode && node != this.root.children) {
      node = node.parentNode;
      while (node.hasAttribute("expanded") && !node.hasAttribute("empty")) {
        if (!node.nextSibling.firstChild) {
          break;
        }
        node = node.nextSibling.firstChild.firstChild;
      }
      return JSON.parse(node.getAttribute("data-id"));
    }
    return null;
  },

  clearSelection: function() {
    this.selectedItem = -1;
  },

  /**
   * Adds an item in the tree. The item can be added as a child to any node in
   * the tree. The method will also create any subnode not present in the
   * process.
   *
   * @param {[string|object]} items
   *        An array of either string or objects where each increasing index
   *        represents an item corresponding to an equivalent depth in the tree.
   *        Each array element can be either just a string with the value as the
   *        id of of that item as well as the display value, or it can be an
   *        object with the following propeties:
   *          - id {string} The id of the item
   *          - label {string} The display value of the item
   *          - node {DOMNode} The dom node if you want to insert some custom
   *                 element as the item. The label property is not used in this
   *                 case
   *          - attachment {object} Any object to be associated with this item.
   *          - type {string} The type of this particular item. If this is null,
   *                 then defaultType will be used.
   *        For example, if items = ["foo", "bar", { id: "id1", label: "baz" }]
   *        and the tree is empty, then the following hierarchy will be created
   *        in the tree:
   *        foo
   *         └ bar
   *            └ baz
   *        Passing the string id instead of the complete object helps when you
   *        are simply adding children to an already existing node and you know
   *        its id.
   */
  add: function(items) {
    this.root.add(items, this.defaultType, this.sorted);
    for (let i = 0; i < items.length; i++) {
      if (items[i].attachment) {
        this.attachments.set(JSON.stringify(
          items.slice(0, i + 1).map(item => item.id || item)
        ), items[i].attachment);
      }
    }
    // Empty the empty-tree-text
    this.setPlaceholderText("");
  },

  /**
   * Check if an item exists.
   *
   * @param {array} item
   *        The array of ids leading up to the item.
   */
  exists: function(item) {
    let bookmark = this.root;

    for (let id of item) {
      if (bookmark.items.has(id)) {
        bookmark = bookmark.items.get(id);
      } else {
        return false;
      }
    }
    return true;
  },

  /**
   * Removes the specified item and all of its child items from the tree.
   *
   * @param {array} item
   *        The array of ids leading up to the item.
   */
  remove: function(item) {
    this.root.remove(item);
    this.attachments.delete(JSON.stringify(item));
    // Display the empty tree text
    if (this.root.items.size == 0 && this.emptyText) {
      this.setPlaceholderText(this.emptyText);
    }
  },

  /**
   * Removes all of the child nodes from this tree.
   */
  clear: function() {
    this.root.remove();
    this.setupRoot();
    this.attachments.clear();
    if (this.emptyText) {
      this.setPlaceholderText(this.emptyText);
    }
  },

  /**
   * Expands the tree completely
   */
  expandAll: function() {
    this.root.expandAll();
  },

  /**
   * Collapses the tree completely
   */
  collapseAll: function() {
    this.root.collapseAll();
  },

  /**
   * Click handler for the tree. Used to select, open and close the tree nodes.
   */
  onClick: function(event) {
    let target = event.originalTarget;
    while (target && !target.classList.contains("tree-widget-item")) {
      if (target == this.root.children) {
        return;
      }
      target = target.parentNode;
    }
    if (!target) {
      return;
    }

    if (target.hasAttribute("expanded")) {
      target.removeAttribute("expanded");
    } else {
      target.setAttribute("expanded", "true");
    }

    if (this._selectedLabel != target) {
      let ids = target.parentNode.getAttribute("data-id");
      this.selectedItem = JSON.parse(ids);
    }
  },

  /**
   * Keydown handler for this tree. Used to select next and previous visible
   * items, as well as collapsing and expanding any item.
   */
  onKeydown: function(event) {
    switch (event.keyCode) {
      case KeyCodes.DOM_VK_UP:
        this.selectPreviousItem();
        break;

      case KeyCodes.DOM_VK_DOWN:
        this.selectNextItem();
        break;

      case KeyCodes.DOM_VK_RIGHT:
        if (this._selectedLabel.hasAttribute("expanded")) {
          this.selectNextItem();
        } else {
          this._selectedLabel.setAttribute("expanded", "true");
        }
        break;

      case KeyCodes.DOM_VK_LEFT:
        if (this._selectedLabel.hasAttribute("expanded") &&
            !this._selectedLabel.hasAttribute("empty")) {
          this._selectedLabel.removeAttribute("expanded");
        } else {
          this.selectPreviousItem();
        }
        break;

      default: return;
    }
    event.preventDefault();
  },

  /**
   * Scrolls the viewport of the tree so that the selected item is always
   * visible.
   */
  ensureSelectedVisible: function() {
    let {top, bottom} = this._selectedLabel.getBoundingClientRect();
    let height = this.root.children.parentNode.clientHeight;
    if (top < 0) {
      this._selectedLabel.scrollIntoView();
    } else if (bottom > height) {
      this._selectedLabel.scrollIntoView(false);
    }
  }
};

module.exports.TreeWidget = TreeWidget;

/**
 * Any item in the tree. This can be an empty leaf node also.
 *
 * @param {HTMLDocument} document
 *        The document element used for creating new nodes.
 * @param {TreeItem} parent
 *        The parent item for this item.
 * @param {string|DOMElement} label
 *        Either the dom node to be used as the item, or the string to be
 *        displayed for this node in the tree
 * @param {string} type
 *        The type of the current node. For ex. "js"
 */
function TreeItem(document, parent, label, type) {
  this.document = document;
  this.node = this.document.createElementNS(HTML_NS, "li");
  this.node.setAttribute("tabindex", "0");
  this.isRoot = !parent;
  this.parent = parent;
  if (this.parent) {
    this.level = this.parent.level + 1;
  }
  if (label) {
    this.label = this.document.createElementNS(HTML_NS, "div");
    this.label.setAttribute("empty", "true");
    this.label.setAttribute("level", this.level);
    this.label.className = "tree-widget-item";
    if (type) {
      this.label.setAttribute("type", type);
    }
    if (typeof label == "string") {
      this.label.textContent = label;
    } else {
      this.label.appendChild(label);
    }
    this.node.appendChild(this.label);
  }
  this.children = this.document.createElementNS(HTML_NS, "ul");
  if (this.isRoot) {
    this.children.className = "tree-widget-container";
  } else {
    this.children.className = "tree-widget-children";
  }
  this.node.appendChild(this.children);
  this.items = new Map();
}

TreeItem.prototype = {

  items: null,

  isSelected: false,

  expanded: false,

  isRoot: false,

  parent: null,

  children: null,

  level: 0,

  /**
   * Adds the item to the sub tree contained by this node. The item to be
   * inserted can be a direct child of this node, or further down the tree.
   *
   * @param {array} items
   *        Same as TreeWidget.add method's argument
   * @param {string} defaultType
   *        The default type of the item to be used when items[i].type is null
   * @param {boolean} sorted
   *        true if the tree items are inserted in a lexically sorted manner.
   *        Otherwise, false if the item are to be appended to their parent.
   */
  add: function(items, defaultType, sorted) {
    if (items.length == this.level) {
      // This is the exit condition of recursive TreeItem.add calls
      return;
    }
    // Get the id and label corresponding to this level inside the tree.
    let id = items[this.level].id || items[this.level];
    if (this.items.has(id)) {
      // An item with same id already exists, thus calling the add method of
      // that child to add the passed node at correct position.
      this.items.get(id).add(items, defaultType, sorted);
      return;
    }
    // No item with the id `id` exists, so we create one and call the add
    // method of that item.
    // The display string of the item can be the label, the id, or the item
    // itself if its a plain string.
    let label = items[this.level].label ||
                items[this.level].id ||
                items[this.level];
    let node = items[this.level].node;
    if (node) {
      // The item is supposed to be a DOMNode, so we fetch the textContent in
      // order to find the correct sorted location of this new item.
      label = node.textContent;
    }
    let treeItem = new TreeItem(this.document, this, node || label,
                                items[this.level].type || defaultType);

    treeItem.add(items, defaultType, sorted);
    treeItem.node.setAttribute("data-id", JSON.stringify(
      items.slice(0, this.level + 1).map(item => item.id || item)
    ));

    if (sorted) {
      // Inserting this newly created item at correct position
      let nextSibling = [...this.items.values()].find(child => {
        return child.label.textContent >= label;
      });

      if (nextSibling) {
        this.children.insertBefore(treeItem.node, nextSibling.node);
      } else {
        this.children.appendChild(treeItem.node);
      }
    } else {
      this.children.appendChild(treeItem.node);
    }

    if (this.label) {
      this.label.removeAttribute("empty");
    }
    this.items.set(id, treeItem);
  },

  /**
   * If this item is to be removed, then removes this item and thus all of its
   * subtree. Otherwise, call the remove method of appropriate child. This
   * recursive method goes on till we have reached the end of the branch or the
   * current item is to be removed.
   *
   * @param {array} items
   *        Ids of items leading up to the item to be removed.
   */
  remove: function(items = []) {
    let id = items.shift();
    if (id && this.items.has(id)) {
      let deleted = this.items.get(id);
      if (!items.length) {
        this.items.delete(id);
      }
      if (this.items.size == 0) {
        this.label.setAttribute("empty", "true");
      }
      deleted.remove(items);
    } else if (!id) {
      this.destroy();
    }
  },

  /**
   * If this item is to be selected, then selected and expands the item.
   * Otherwise, if a child item is to be selected, just expands this item.
   *
   * @param {array} items
   *        Ids of items leading up to the item to be selected.
   */
  setSelectedItem: function(items) {
    if (!items[this.level]) {
      this.label.classList.add("theme-selected");
      this.label.setAttribute("expanded", "true");
      return this.label;
    }
    if (this.items.has(items[this.level])) {
      let label = this.items.get(items[this.level]).setSelectedItem(items);
      if (label && this.label) {
        this.label.setAttribute("expanded", true);
      }
      return label;
    }
    return null;
  },

  /**
   * Collapses this item and all of its sub tree items
   */
  collapseAll: function() {
    if (this.label) {
      this.label.removeAttribute("expanded");
    }
    for (let child of this.items.values()) {
      child.collapseAll();
    }
  },

  /**
   * Expands this item and all of its sub tree items
   */
  expandAll: function() {
    if (this.label) {
      this.label.setAttribute("expanded", "true");
    }
    for (let child of this.items.values()) {
      child.expandAll();
    }
  },

  destroy: function() {
    this.children.remove();
    this.node.remove();
    this.label = null;
    this.items = null;
    this.children = null;
  }
};
