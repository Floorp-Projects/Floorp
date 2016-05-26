/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { ViewHelpers } = require("devtools/client/shared/widgets/view-helpers");

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
  "resource://devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/Console.jsm");

this.EXPORTED_SYMBOLS = ["AbstractTreeItem"];

/**
 * A very generic and low-level tree view implementation. It is not intended
 * to be used alone, but as a base class that you can extend to build your
 * own custom implementation.
 *
 * Language:
 *   - An "item" is an instance of an AbstractTreeItem.
 *   - An "element" or "node" is an nsIDOMNode.
 *
 * The following events are emitted by this tree, always from the root item,
 * with the first argument pointing to the affected child item:
 *   - "expand": when an item is expanded in the tree
 *   - "collapse": when an item is collapsed in the tree
 *   - "focus": when an item is selected in the tree
 *
 * For example, you can extend this abstract class like this:
 *
 * function MyCustomTreeItem(dataSrc, properties) {
 *   AbstractTreeItem.call(this, properties);
 *   this.itemDataSrc = dataSrc;
 * }
 *
 * MyCustomTreeItem.prototype = Heritage.extend(AbstractTreeItem.prototype, {
 *   _displaySelf: function(document, arrowNode) {
 *     let node = document.createElement("hbox");
 *     ...
 *     // Append the provided arrow node wherever you want.
 *     node.appendChild(arrowNode);
 *     ...
 *     // Use `this.itemDataSrc` to customize the tree item and
 *     // `this.level` to calculate the indentation.
 *     node.style.marginInlineStart = (this.level * 10) + "px";
 *     node.appendChild(document.createTextNode(this.itemDataSrc.label));
 *     ...
 *     return node;
 *   },
 *   _populateSelf: function(children) {
 *     ...
 *     // Use `this.itemDataSrc` to get the data source for the child items.
 *     let someChildDataSrc = this.itemDataSrc.children[0];
 *     ...
 *     children.push(new MyCustomTreeItem(someChildDataSrc, {
 *       parent: this,
 *       level: this.level + 1
 *     }));
 *     ...
 *   }
 * });
 *
 * And then you could use it like this:
 *
 * let dataSrc = {
 *   label: "root",
 *   children: [{
 *     label: "foo",
 *     children: []
 *   }, {
 *     label: "bar",
 *     children: [{
 *       label: "baz",
 *       children: []
 *     }]
 *   }]
 * };
 * let root = new MyCustomTreeItem(dataSrc, { parent: null });
 * root.attachTo(nsIDOMNode);
 * root.expand();
 *
 * The following tree view will be generated (after expanding all nodes):
 * ▼ root
 *   ▶ foo
 *   ▼ bar
 *     ▶ baz
 *
 * The way the data source is implemented is completely up to you. There's
 * no assumptions made and you can use it however you like inside the
 * `_displaySelf` and `populateSelf` methods. If you need to add children to a
 * node at a later date, you just need to modify the data source:
 *
 * dataSrc[...path-to-foo...].children.push({
 *   label: "lazily-added-node"
 *   children: []
 * });
 *
 * The existing tree view will be modified like so (after expanding `foo`):
 * ▼ root
 *   ▼ foo
 *     ▶ lazily-added-node
 *   ▼ bar
 *     ▶ baz
 *
 * Everything else is taken care of automagically!
 *
 * @param AbstractTreeItem parent
 *        The parent tree item. Should be null for root items.
 * @param number level
 *        The indentation level in the tree. The root item is at level 0.
 */
function AbstractTreeItem({ parent, level }) {
  this._rootItem = parent ? parent._rootItem : this;
  this._parentItem = parent;
  this._level = level || 0;
  this._childTreeItems = [];

  // Events are always propagated through the root item. Decorating every
  // tree item as an event emitter is a very costly operation.
  if (this == this._rootItem) {
    EventEmitter.decorate(this);
  }
}
this.AbstractTreeItem = AbstractTreeItem;

AbstractTreeItem.prototype = {
  _containerNode: null,
  _targetNode: null,
  _arrowNode: null,
  _constructed: false,
  _populated: false,
  _expanded: false,

  /**
   * Optionally, trees may be allowed to automatically expand a few levels deep
   * to avoid initially displaying a completely collapsed tree.
   */
  autoExpandDepth: 0,

  /**
   * Creates the view for this tree item. Implement this method in the
   * inheriting classes to create the child node displayed in the tree.
   * Use `this.level` and the provided `arrowNode` as you see fit.
   *
   * @param nsIDOMNode document
   * @param nsIDOMNode arrowNode
   * @return nsIDOMNode
   */
  _displaySelf: function (document, arrowNode) {
    throw new Error(
      "The `_displaySelf` method needs to be implemented by inheriting classes.");
  },

  /**
   * Populates this tree item with child items, whenever it's expanded.
   * Implement this method in the inheriting classes to fill the provided
   * `children` array with AbstractTreeItem instances, which will then be
   * magically handled by this tree item.
   *
   * @param array:AbstractTreeItem children
   */
  _populateSelf: function (children) {
    throw new Error(
      "The `_populateSelf` method needs to be implemented by inheriting classes.");
  },

  /**
   * Gets the this tree's owner document.
   * @return Document
   */
  get document() {
    return this._containerNode.ownerDocument;
  },

  /**
   * Gets the root item of this tree.
   * @return AbstractTreeItem
   */
  get root() {
    return this._rootItem;
  },

  /**
   * Gets the parent of this tree item.
   * @return AbstractTreeItem
   */
  get parent() {
    return this._parentItem;
  },

  /**
   * Gets the indentation level of this tree item.
   */
  get level() {
    return this._level;
  },

  /**
   * Gets the element displaying this tree item.
   */
  get target() {
    return this._targetNode;
  },

  /**
   * Gets the element containing all tree items.
   * @return nsIDOMNode
   */
  get container() {
    return this._containerNode;
  },

  /**
   * Returns whether or not this item is populated in the tree.
   * Collapsed items can still be populated.
   * @return boolean
   */
  get populated() {
    return this._populated;
  },

  /**
   * Returns whether or not this item is expanded in the tree.
   * Expanded items with no children aren't consudered `populated`.
   * @return boolean
   */
  get expanded() {
    return this._expanded;
  },

  /**
   * Gets the bounds for this tree's container without flushing.
   * @return object
   */
  get bounds() {
    let win = this.document.defaultView;
    let utils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    return utils.getBoundsWithoutFlushing(this._containerNode);
  },

  /**
   * Creates and appends this tree item to the specified parent element.
   *
   * @param nsIDOMNode containerNode
   *        The parent element for this tree item (and every other tree item).
   * @param nsIDOMNode fragmentNode [optional]
   *        An optional document fragment temporarily holding this tree item in
   *        the current batch. Defaults to the `containerNode`.
   * @param nsIDOMNode beforeNode [optional]
   *        An optional child element which should succeed this tree item.
   */
  attachTo: function (containerNode, fragmentNode = containerNode, beforeNode = null) {
    this._containerNode = containerNode;
    this._constructTargetNode();

    if (beforeNode) {
      fragmentNode.insertBefore(this._targetNode, beforeNode);
    } else {
      fragmentNode.appendChild(this._targetNode);
    }

    if (this._level < this.autoExpandDepth) {
      this.expand();
    }
  },

  /**
   * Permanently removes this tree item (and all subsequent children) from the
   * parent container.
   */
  remove: function () {
    this._targetNode.remove();
    this._hideChildren();
    this._childTreeItems.length = 0;
  },

  /**
   * Focuses this item in the tree.
   */
  focus: function () {
    this._targetNode.focus();
  },

  /**
   * Expands this item in the tree.
   */
  expand: function () {
    if (this._expanded) {
      return;
    }
    this._expanded = true;
    this._arrowNode.setAttribute("open", "");
    this._targetNode.setAttribute("expanded", "");
    this._toggleChildren(true);
    this._rootItem.emit("expand", this);
  },

  /**
   * Collapses this item in the tree.
   */
  collapse: function () {
    if (!this._expanded) {
      return;
    }
    this._expanded = false;
    this._arrowNode.removeAttribute("open");
    this._targetNode.removeAttribute("expanded", "");
    this._toggleChildren(false);
    this._rootItem.emit("collapse", this);
  },

  /**
   * Returns the child item at the specified index.
   *
   * @param number index
   * @return AbstractTreeItem
   */
  getChild: function (index = 0) {
    return this._childTreeItems[index];
  },

  /**
   * Calls the provided function on all the descendants of this item.
   * If this item was never expanded, then no descendents exist yet.
   * @param function cb
   */
  traverse: function (cb) {
    for (let child of this._childTreeItems) {
      cb(child);
      child.bfs();
    }
  },

  /**
   * Calls the provided function on all descendants of this item until
   * a truthy value is returned by the predicate.
   * @param function predicate
   * @return AbstractTreeItem
   */
  find: function (predicate) {
    for (let child of this._childTreeItems) {
      if (predicate(child) || child.find(predicate)) {
        return child;
      }
    }
    return null;
  },

  /**
   * Shows or hides all the children of this item in the tree. If neessary,
   * populates this item with children.
   *
   * @param boolean visible
   *        True if the children should be visible, false otherwise.
   */
  _toggleChildren: function (visible) {
    if (visible) {
      if (!this._populated) {
        this._populateSelf(this._childTreeItems);
        this._populated = this._childTreeItems.length > 0;
      }
      this._showChildren();
    } else {
      this._hideChildren();
    }
  },

  /**
   * Shows all children of this item in the tree.
   */
  _showChildren: function () {
    // If this is the root item and we're not expanding any child nodes,
    // it is safe to append everything at once.
    if (this == this._rootItem && this.autoExpandDepth == 0) {
      this._appendChildrenBatch();
    }
    // Otherwise, append the child items and their descendants successively;
    // if not, the tree will become garbled and nodes will intertwine,
    // since all the tree items are sharing a single container node.
    else {
      this._appendChildrenSuccessive();
    }
  },

  /**
   * Hides all children of this item in the tree.
   */
  _hideChildren: function () {
    for (let item of this._childTreeItems) {
      item._targetNode.remove();
      item._hideChildren();
    }
  },

  /**
   * Appends all children in a single batch.
   * This only works properly for root nodes when no child nodes will expand.
   */
  _appendChildrenBatch: function () {
    if (this._fragment === undefined) {
      this._fragment = this.document.createDocumentFragment();
    }

    let childTreeItems = this._childTreeItems;

    for (let i = 0, len = childTreeItems.length; i < len; i++) {
      childTreeItems[i].attachTo(this._containerNode, this._fragment);
    }

    this._containerNode.appendChild(this._fragment);
  },

  /**
   * Appends all children successively.
   */
  _appendChildrenSuccessive: function () {
    let childTreeItems = this._childTreeItems;
    let expandedChildTreeItems = childTreeItems.filter(e => e._expanded);
    let nextNode = this._getSiblingAtDelta(1);

    for (let i = 0, len = childTreeItems.length; i < len; i++) {
      childTreeItems[i].attachTo(this._containerNode, undefined, nextNode);
    }
    for (let i = 0, len = expandedChildTreeItems.length; i < len; i++) {
      expandedChildTreeItems[i]._showChildren();
    }
  },

  /**
   * Constructs and stores the target node displaying this tree item.
   */
  _constructTargetNode: function () {
    if (this._constructed) {
      return;
    }
    this._onArrowClick = this._onArrowClick.bind(this);
    this._onClick = this._onClick.bind(this);
    this._onDoubleClick = this._onDoubleClick.bind(this);
    this._onKeyPress = this._onKeyPress.bind(this);
    this._onFocus = this._onFocus.bind(this);
    this._onBlur = this._onBlur.bind(this);

    let document = this.document;

    let arrowNode = this._arrowNode = document.createElement("hbox");
    arrowNode.className = "arrow theme-twisty";
    arrowNode.addEventListener("mousedown", this._onArrowClick);

    let targetNode = this._targetNode = this._displaySelf(document, arrowNode);
    targetNode.style.MozUserFocus = "normal";

    targetNode.addEventListener("mousedown", this._onClick);
    targetNode.addEventListener("dblclick", this._onDoubleClick);
    targetNode.addEventListener("keypress", this._onKeyPress);
    targetNode.addEventListener("focus", this._onFocus);
    targetNode.addEventListener("blur", this._onBlur);

    this._constructed = true;
  },

  /**
   * Gets the element displaying an item in the tree at the specified offset
   * relative to this item.
   *
   * @param number delta
   *        The offset from this item to the target item.
   * @return nsIDOMNode
   *         The element displaying the target item at the specified offset.
   */
  _getSiblingAtDelta: function (delta) {
    let childNodes = this._containerNode.childNodes;
    let indexOfSelf = Array.indexOf(childNodes, this._targetNode);
    return childNodes[indexOfSelf + delta];
  },

  /**
   * Focuses the next item in this tree.
   */
  _focusNextNode: function () {
    let nextElement = this._getSiblingAtDelta(1);
    if (nextElement) nextElement.focus(); // nsIDOMNode
  },

  /**
   * Focuses the previous item in this tree.
   */
  _focusPrevNode: function () {
    let prevElement = this._getSiblingAtDelta(-1);
    if (prevElement) prevElement.focus(); // nsIDOMNode
  },

  /**
   * Focuses the parent item in this tree.
   *
   * The parent item is not always the previous item, because any tree item
   * may have multiple children.
   */
  _focusParentNode: function () {
    let parentItem = this._parentItem;
    if (parentItem) parentItem.focus(); // AbstractTreeItem
  },

  /**
   * Handler for the "click" event on the arrow node of this tree item.
   */
  _onArrowClick: function (e) {
    if (!this._expanded) {
      this.expand();
    } else {
      this.collapse();
    }
  },

  /**
   * Handler for the "click" event on the element displaying this tree item.
   */
  _onClick: function (e) {
    e.stopPropagation();
    this.focus();
  },

  /**
   * Handler for the "dblclick" event on the element displaying this tree item.
   */
  _onDoubleClick: function (e) {
    // Ignore dblclick on the arrow as it has already recived and handled two
    // click events.
    if (!e.target.classList.contains("arrow")) {
      this._onArrowClick(e);
    }
    this.focus();
  },

  /**
   * Handler for the "keypress" event on the element displaying this tree item.
   */
  _onKeyPress: function (e) {
    // Prevent scrolling when pressing navigation keys.
    ViewHelpers.preventScrolling(e);

    switch (e.keyCode) {
      case e.DOM_VK_UP:
        this._focusPrevNode();
        return;

      case e.DOM_VK_DOWN:
        this._focusNextNode();
        return;

      case e.DOM_VK_LEFT:
        if (this._expanded && this._populated) {
          this.collapse();
        } else {
          this._focusParentNode();
        }
        return;

      case e.DOM_VK_RIGHT:
        if (!this._expanded) {
          this.expand();
        } else {
          this._focusNextNode();
        }
        return;
    }
  },

  /**
   * Handler for the "focus" event on the element displaying this tree item.
   */
  _onFocus: function (e) {
    this._rootItem.emit("focus", this);
  },

  /**
   * Handler for the "blur" event on the element displaying this tree item.
   */
  _onBlur: function (e) {
    this._rootItem.emit("blur", this);
  }
};
