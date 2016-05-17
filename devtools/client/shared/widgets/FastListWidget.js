/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { Cu, Ci } = require("chrome");
const { ViewHelpers } = require("devtools/client/shared/widgets/view-helpers");

/**
 * A list menu widget that attempts to be very fast.
 *
 * Note: this widget should be used in tandem with the WidgetMethods in
 * view-helpers.js.
 *
 * @param nsIDOMNode aNode
 *        The element associated with the widget.
 */
const FastListWidget = module.exports = function FastListWidget(aNode) {
  this.document = aNode.ownerDocument;
  this.window = this.document.defaultView;
  this._parent = aNode;
  this._fragment = this.document.createDocumentFragment();

  // This is a prototype element that each item added to the list clones.
  this._templateElement = this.document.createElement("hbox");

  // Create an internal scrollbox container.
  this._list = this.document.createElement("scrollbox");
  this._list.className = "fast-list-widget-container theme-body";
  this._list.setAttribute("flex", "1");
  this._list.setAttribute("orient", "vertical");
  this._list.setAttribute("tabindex", "0");
  this._list.addEventListener("keypress", e => this.emit("keyPress", e), false);
  this._list.addEventListener("mousedown", e => this.emit("mousePress", e), false);
  this._parent.appendChild(this._list);

  this._orderedMenuElementsArray = [];
  this._itemsByElement = new Map();

  // This widget emits events that can be handled in a MenuContainer.
  EventEmitter.decorate(this);

  // Delegate some of the associated node's methods to satisfy the interface
  // required by MenuContainer instances.
  ViewHelpers.delegateWidgetAttributeMethods(this, aNode);
  ViewHelpers.delegateWidgetEventMethods(this, aNode);
};

FastListWidget.prototype = {
  /**
   * Inserts an item in this container at the specified index, optionally
   * grouping by name.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param nsIDOMNode aContents
   *        The node to be displayed in the container.
   * @param Object aAttachment [optional]
   *        Extra data for the user.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertItemAt: function (aIndex, aContents, aAttachment = {}) {
    let element = this._templateElement.cloneNode();
    element.appendChild(aContents);

    if (aIndex >= 0) {
      throw new Error("FastListWidget only supports appending items.");
    }

    this._fragment.appendChild(element);
    this._orderedMenuElementsArray.push(element);
    this._itemsByElement.set(element, this);

    return element;
  },

  /**
   * This is a non-standard widget implementation method. When appending items,
   * they are queued in a document fragment. This method appends the document
   * fragment to the dom.
   */
  flush: function () {
    this._list.appendChild(this._fragment);
  },

  /**
   * Removes all of the child nodes from this container.
   */
  removeAllItems: function () {
    let parent = this._parent;
    let list = this._list;

    while (list.hasChildNodes()) {
      list.firstChild.remove();
    }

    this._selectedItem = null;

    this._orderedMenuElementsArray.length = 0;
    this._itemsByElement.clear();
  },

  /**
   * Remove the given item.
   */
  removeChild: function (child) {
    throw new Error("Not yet implemented");
  },

  /**
   * Gets the currently selected child node in this container.
   * @return nsIDOMNode
   */
  get selectedItem() {
    return this._selectedItem;
  },

  /**
   * Sets the currently selected child node in this container.
   * @param nsIDOMNode child
   */
  set selectedItem(child) {
    let menuArray = this._orderedMenuElementsArray;

    if (!child) {
      this._selectedItem = null;
    }
    for (let node of menuArray) {
      if (node == child) {
        node.classList.add("selected");
        this._selectedItem = node;
      } else {
        node.classList.remove("selected");
      }
    }

    this.ensureElementIsVisible(this.selectedItem);
  },

  /**
   * Returns the child node in this container situated at the specified index.
   *
   * @param number index
   *        The position in the container intended for this item.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  getItemAtIndex: function (index) {
    return this._orderedMenuElementsArray[index];
  },

  /**
   * Adds a new attribute or changes an existing attribute on this container.
   *
   * @param string name
   *        The name of the attribute.
   * @param string value
   *        The desired attribute value.
   */
  setAttribute: function (name, value) {
    this._parent.setAttribute(name, value);

    if (name == "emptyText") {
      this._textWhenEmpty = value;
    }
  },

  /**
   * Removes an attribute on this container.
   *
   * @param string name
   *        The name of the attribute.
   */
  removeAttribute: function (name) {
    this._parent.removeAttribute(name);

    if (name == "emptyText") {
      this._removeEmptyText();
    }
  },

  /**
   * Ensures the specified element is visible.
   *
   * @param nsIDOMNode element
   *        The element to make visible.
   */
  ensureElementIsVisible: function (element) {
    if (!element) {
      return;
    }

    // Ensure the element is visible but not scrolled horizontally.
    let boxObject = this._list.boxObject;
    boxObject.ensureElementIsVisible(element);
    boxObject.scrollBy(-this._list.clientWidth, 0);
  },

  /**
   * Sets the text displayed in this container when empty.
   * @param string aValue
   */
  set _textWhenEmpty(aValue) {
    if (this._emptyTextNode) {
      this._emptyTextNode.setAttribute("value", aValue);
    }
    this._emptyTextValue = aValue;
    this._showEmptyText();
  },

  /**
   * Creates and appends a label signaling that this container is empty.
   */
  _showEmptyText: function () {
    if (this._emptyTextNode || !this._emptyTextValue) {
      return;
    }
    let label = this.document.createElement("label");
    label.className = "plain fast-list-widget-empty-text";
    label.setAttribute("value", this._emptyTextValue);

    this._parent.insertBefore(label, this._list);
    this._emptyTextNode = label;
  },

  /**
   * Removes the label signaling that this container is empty.
   */
  _removeEmptyText: function () {
    if (!this._emptyTextNode) {
      return;
    }
    this._parent.removeChild(this._emptyTextNode);
    this._emptyTextNode = null;
  },

  window: null,
  document: null,
  _parent: null,
  _list: null,
  _selectedItem: null,
  _orderedMenuElementsArray: null,
  _itemsByElement: null,
  _emptyTextNode: null,
  _emptyTextValue: ""
};
