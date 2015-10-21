/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");

this.EXPORTED_SYMBOLS = ["SimpleListWidget"];

/**
 * A very simple vertical list view.
 *
 * Note: this widget should be used in tandem with the WidgetMethods in
 * ViewHelpers.jsm.
 *
 * @param nsIDOMNode aNode
 *        The element associated with the widget.
 */
function SimpleListWidget(aNode) {
  this.document = aNode.ownerDocument;
  this.window = this.document.defaultView;
  this._parent = aNode;

  // Create an internal list container.
  this._list = this.document.createElement("scrollbox");
  this._list.className = "simple-list-widget-container theme-body";
  this._list.setAttribute("flex", "1");
  this._list.setAttribute("orient", "vertical");
  this._parent.appendChild(this._list);

  // Delegate some of the associated node's methods to satisfy the interface
  // required by WidgetMethods instances.
  ViewHelpers.delegateWidgetAttributeMethods(this, aNode);
  ViewHelpers.delegateWidgetEventMethods(this, aNode);
}

SimpleListWidget.prototype = {
  /**
   * Inserts an item in this container at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param nsIDOMNode aContents
   *        The node displayed in the container.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertItemAt: function(aIndex, aContents) {
    aContents.classList.add("simple-list-widget-item");

    let list = this._list;
    return list.insertBefore(aContents, list.childNodes[aIndex]);
  },

  /**
   * Returns the child node in this container situated at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  getItemAtIndex: function(aIndex) {
    return this._list.childNodes[aIndex];
  },

  /**
   * Immediately removes the specified child node from this container.
   *
   * @param nsIDOMNode aChild
   *        The element associated with the displayed item.
   */
  removeChild: function(aChild) {
    this._list.removeChild(aChild);

    if (this._selectedItem == aChild) {
      this._selectedItem = null;
    }
  },

  /**
   * Removes all of the child nodes from this container.
   */
  removeAllItems: function() {
    let list = this._list;
    let parent = this._parent;

    while (list.hasChildNodes()) {
      list.firstChild.remove();
    }

    parent.scrollTop = 0;
    parent.scrollLeft = 0;
    this._selectedItem = null;
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
   * @param nsIDOMNode aChild
   */
  set selectedItem(aChild) {
    let childNodes = this._list.childNodes;

    if (!aChild) {
      this._selectedItem = null;
    }
    for (let node of childNodes) {
      if (node == aChild) {
        node.classList.add("selected");
        this._selectedItem = node;
      } else {
        node.classList.remove("selected");
      }
    }
  },

  /**
   * Adds a new attribute or changes an existing attribute on this container.
   *
   * @param string aName
   *        The name of the attribute.
   * @param string aValue
   *        The desired attribute value.
   */
  setAttribute: function(aName, aValue) {
    this._parent.setAttribute(aName, aValue);

    if (aName == "emptyText") {
      this._textWhenEmpty = aValue;
    } else if (aName == "headerText") {
      this._textAsHeader = aValue;
    }
  },

  /**
   * Removes an attribute on this container.
   *
   * @param string aName
   *        The name of the attribute.
   */
  removeAttribute: function(aName) {
    this._parent.removeAttribute(aName);

    if (aName == "emptyText") {
      this._removeEmptyText();
    }
  },

  /**
   * Ensures the specified element is visible.
   *
   * @param nsIDOMNode aElement
   *        The element to make visible.
   */
  ensureElementIsVisible: function(aElement) {
    if (!aElement) {
      return;
    }

    // Ensure the element is visible but not scrolled horizontally.
    let boxObject = this._list.boxObject;
    boxObject.ensureElementIsVisible(aElement);
    boxObject.scrollBy(-this._list.clientWidth, 0);
  },

  /**
   * Sets the text displayed permanently in this container as a header.
   * @param string aValue
   */
  set _textAsHeader(aValue) {
    if (this._headerTextNode) {
      this._headerTextNode.setAttribute("value", aValue);
    }
    this._headerTextValue = aValue;
    this._showHeaderText();
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
   * Creates and appends a label displayed as this container's header.
   */
  _showHeaderText: function() {
    if (this._headerTextNode || !this._headerTextValue) {
      return;
    }
    let label = this.document.createElement("label");
    label.className = "plain simple-list-widget-perma-text";
    label.setAttribute("value", this._headerTextValue);

    this._parent.insertBefore(label, this._list);
    this._headerTextNode = label;
  },

  /**
   * Creates and appends a label signaling that this container is empty.
   */
  _showEmptyText: function() {
    if (this._emptyTextNode || !this._emptyTextValue) {
      return;
    }
    let label = this.document.createElement("label");
    label.className = "plain simple-list-widget-empty-text";
    label.setAttribute("value", this._emptyTextValue);

    this._parent.appendChild(label);
    this._emptyTextNode = label;
  },

  /**
   * Removes the label signaling that this container is empty.
   */
  _removeEmptyText: function() {
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
  _headerTextNode: null,
  _headerTextValue: "",
  _emptyTextNode: null,
  _emptyTextValue: ""
};
