/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const ENSURE_SELECTION_VISIBLE_DELAY = 50; // ms

this.EXPORTED_SYMBOLS = ["BreadcrumbsWidget"];

/**
 * A breadcrumb-like list of items.
 *
 * @param nsIDOMNode aNode
 *        The element associated with the widget.
 */
this.BreadcrumbsWidget = function BreadcrumbsWidget(aNode) {
  this._parent = aNode;

  // Create an internal arrowscrollbox container.
  this._list = this.document.createElement("arrowscrollbox");
  this._list.id = "inspector-breadcrumbs";
  this._list.setAttribute("flex", "1");
  this._list.setAttribute("orient", "horizontal");
  this._list.setAttribute("clicktoscroll", "true")
  this._parent.appendChild(this._list);

  // By default, hide the arrows. We let the arrowscrollbox show them
  // in case of overflow.
  this._list._scrollButtonUp.collapsed = true;
  this._list._scrollButtonDown.collapsed = true;
  this._list.addEventListener("underflow", this._onUnderflow, false);
  this._list.addEventListener("overflow", this._onOverflow, false);
};

BreadcrumbsWidget.prototype = {
  /**
   * Inserts an item in this container at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param string | nsIDOMNode aContents
   *        The string or node displayed in the container.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertItemAt: function BCW_insertItemAt(aIndex, aContents) {
    let list = this._list;
    let breadcrumb = new Breadcrumb(this);
    breadcrumb.contents = aContents;

    return list.insertBefore(breadcrumb.target, list.childNodes[aIndex]);
  },

  /**
   * Returns the child node in this container situated at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  getItemAtIndex: function BCW_getItemAtIndex(aIndex) {
    return this._list.childNodes[aIndex];
  },

  /**
   * Removes the specified child node from this container.
   *
   * @param nsIDOMNode aChild
   *        The element associated with the displayed item.
   */
  removeChild: function BCW_removeChild(aChild) {
    this._list.removeChild(aChild);

    if (this._selectedItem == aChild) {
      this._selectedItem = null;
    }
  },

  /**
   * Removes all of the child nodes from this container.
   */
  removeAllItems: function BCW_removeAllItems() {
    let parent = this._parent;
    let list = this._list;
    let firstChild;

    while (firstChild = list.firstChild) {
      list.removeChild(firstChild);
    }
    this._selectedItem = null;
  },

  /**
   * Gets the currently selected child node in this container.
   * @return nsIDOMNode
   */
  get selectedItem() this._selectedItem,

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
        node.setAttribute("checked", "");
        node.classList.add("selected");
        this._selectedItem = node;
      } else {
        node.removeAttribute("checked");
        node.classList.remove("selected");
      }
    }

    // Repeated calls to ensureElementIsVisible would interfere with each other
    // and may sometimes result in incorrect scroll positions.
    this.window.clearTimeout(this._ensureVisibleTimeout);
    this._ensureVisibleTimeout = this.window.setTimeout(function() {
      // Scroll the selected item into view.
      if (this._selectedItem) {
        this._list.ensureElementIsVisible(this._selectedItem);
      }
    }.bind(this), ENSURE_SELECTION_VISIBLE_DELAY);
  },

  /**
   * The underflow and overflow listener for the arrowscrollbox container.
   */
  _onUnderflow: function BCW__onUnderflow({target}) {
    target._scrollButtonUp.collapsed = true;
    target._scrollButtonDown.collapsed = true;
    target.removeAttribute("overflows");
  },

  /**
   * The underflow and overflow listener for the arrowscrollbox container.
   */
  _onOverflow: function BCW__onOverflow({target}) {
    target._scrollButtonUp.collapsed = false;
    target._scrollButtonDown.collapsed = false;
    target.setAttribute("overflows", "");
  },

  /**
   * Gets the parent node holding this view.
   * @return nsIDOMNode
   */
  get parentNode() this._parent,

  /**
   * Gets the owner document holding this view.
   * @return nsIHTMLDocument
   */
  get document() this._parent.ownerDocument,

  /**
   * Gets the default window holding this view.
   * @return nsIDOMWindow
   */
  get window() this.document.defaultView,

  _parent: null,
  _list: null,
  _selectedItem: null
};

/**
 * A Breadcrumb constructor for the BreadcrumbsWidget.
 *
 * @param BreadcrumbsWidget aWidget
 *        The widget to contain this breadcrumb.
 */
function Breadcrumb(aWidget) {
  this.ownerView = aWidget;

  this._target = this.document.createElement("button");
  this._target.className = "inspector-breadcrumbs-button";
  this.parentNode.appendChild(this._target);
}

Breadcrumb.prototype = {
  /**
   * Sets the contents displayed in this item's view.
   *
   * @param string | nsIDOMNode aContents
   *        The string or node displayed in the container.
   */
  set contents(aContents) {
    // If this item's view contents are a string, then create a label to hold
    // the text displayed in this breadcrumb.
    if (typeof aContents == "string") {
      let label = this.document.createElement("label");
      label.setAttribute("value", aContents);
      this.contents = label;
      return;
    }
    // If there are already some contents displayed, replace them.
    if (this._target.hasChildNodes()) {
      this._target.replaceChild(aContents, this._target.firstChild);
      return;
    }
    // These are the first contents ever displayed.
    this._target.appendChild(aContents);
  },

  /**
   * Gets the element associated with this item.
   * @return nsIDOMNode
   */
  get target() this._target,

  /**
   * Gets the parent node holding this scope.
   * @return nsIDOMNode
   */
  get parentNode() this.ownerView._list,

  /**
   * Gets the owner document holding this scope.
   * @return nsIHTMLDocument
   */
  get document() this.ownerView.document,

  /**
   * Gets the default window holding this scope.
   * @return nsIDOMWindow
   */
  get window() this.ownerView.window,

  ownerView: null,
  _target: null
};
