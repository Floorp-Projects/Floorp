/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

const ENSURE_SELECTION_VISIBLE_DELAY = 50; // ms

Cu.import("resource:///modules/devtools/ViewHelpers.jsm");
Cu.import("resource://gre/modules/devtools/event-emitter.js");

this.EXPORTED_SYMBOLS = ["BreadcrumbsWidget"];

/**
 * A breadcrumb-like list of items.
 *
 * Note: this widget should be used in tandem with the WidgetMethods in
 * ViewHelpers.jsm.
 *
 * @param nsIDOMNode aNode
 *        The element associated with the widget.
 * @param Object aOptions
 *        - smoothScroll: specifies if smooth scrolling on selection is enabled.
 */
this.BreadcrumbsWidget = function BreadcrumbsWidget(aNode, aOptions={}) {
  this.document = aNode.ownerDocument;
  this.window = this.document.defaultView;
  this._parent = aNode;

  // Create an internal arrowscrollbox container.
  this._list = this.document.createElement("arrowscrollbox");
  this._list.className = "breadcrumbs-widget-container";
  this._list.setAttribute("flex", "1");
  this._list.setAttribute("orient", "horizontal");
  this._list.setAttribute("clicktoscroll", "true");
  this._list.setAttribute("smoothscroll", !!aOptions.smoothScroll);
  this._list.addEventListener("keypress", e => this.emit("keyPress", e), false);
  this._list.addEventListener("mousedown", e => this.emit("mousePress", e), false);
  this._parent.appendChild(this._list);

  // By default, hide the arrows. We let the arrowscrollbox show them
  // in case of overflow.
  this._list._scrollButtonUp.collapsed = true;
  this._list._scrollButtonDown.collapsed = true;
  this._list.addEventListener("underflow", this._onUnderflow.bind(this), false);
  this._list.addEventListener("overflow", this._onOverflow.bind(this), false);

  // These separators are used for CSS purposes only, and are positioned
  // off screen, but displayed with -moz-element.
  this._separators = this.document.createElement("box");
  this._separators.className = "breadcrumb-separator-container";
  this._separators.innerHTML =
                    "<box id='breadcrumb-separator-before'></box>" +
                    "<box id='breadcrumb-separator-after'></box>" +
                    "<box id='breadcrumb-separator-normal'></box>";
  this._parent.appendChild(this._separators);

  // This widget emits events that can be handled in a MenuContainer.
  EventEmitter.decorate(this);

  // Delegate some of the associated node's methods to satisfy the interface
  // required by MenuContainer instances.
  ViewHelpers.delegateWidgetAttributeMethods(this, aNode);
  ViewHelpers.delegateWidgetEventMethods(this, aNode);
};

BreadcrumbsWidget.prototype = {
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
    let list = this._list;
    let breadcrumb = new Breadcrumb(this, aContents);
    return list.insertBefore(breadcrumb._target, list.childNodes[aIndex]);
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
   * Removes the specified child node from this container.
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

    while (list.hasChildNodes()) {
      list.firstChild.remove();
    }

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
        node.setAttribute("checked", "");
        this._selectedItem = node;
      } else {
        node.removeAttribute("checked");
      }
    }
  },

  /**
   * Returns the value of the named attribute on this container.
   *
   * @param string aName
   *        The name of the attribute.
   * @return string
   *         The current attribute value.
   */
  getAttribute: function(aName) {
    if (aName == "scrollPosition") return this._list.scrollPosition;
    if (aName == "scrollWidth") return this._list.scrollWidth;
    return this._parent.getAttribute(aName);
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

    // Repeated calls to ensureElementIsVisible would interfere with each other
    // and may sometimes result in incorrect scroll positions.
    setNamedTimeout("breadcrumb-select", ENSURE_SELECTION_VISIBLE_DELAY, () => {
      if (this._list.ensureElementIsVisible) {
        this._list.ensureElementIsVisible(aElement);
      }
    });
  },

  /**
   * The underflow and overflow listener for the arrowscrollbox container.
   */
  _onUnderflow: function({ target }) {
    if (target != this._list) {
      return;
    }
    target._scrollButtonUp.collapsed = true;
    target._scrollButtonDown.collapsed = true;
    target.removeAttribute("overflows");
  },

  /**
   * The underflow and overflow listener for the arrowscrollbox container.
   */
  _onOverflow: function({ target }) {
    if (target != this._list) {
      return;
    }
    target._scrollButtonUp.collapsed = false;
    target._scrollButtonDown.collapsed = false;
    target.setAttribute("overflows", "");
  },

  window: null,
  document: null,
  _parent: null,
  _list: null,
  _selectedItem: null
};

/**
 * A Breadcrumb constructor for the BreadcrumbsWidget.
 *
 * @param BreadcrumbsWidget aWidget
 *        The widget to contain this breadcrumb.
 * @param nsIDOMNode aContents
 *        The node displayed in the container.
 */
function Breadcrumb(aWidget, aContents) {
  this.document = aWidget.document;
  this.window = aWidget.window;
  this.ownerView = aWidget;

  this._target = this.document.createElement("hbox");
  this._target.className = "breadcrumbs-widget-item";
  this._target.setAttribute("align", "center");
  this.contents = aContents;
}

Breadcrumb.prototype = {
  /**
   * Sets the contents displayed in this item's view.
   *
   * @param string | nsIDOMNode aContents
   *        The string or node displayed in the container.
   */
  set contents(aContents) {
    // If there are already some contents displayed, replace them.
    if (this._target.hasChildNodes()) {
      this._target.replaceChild(aContents, this._target.firstChild);
      return;
    }
    // These are the first contents ever displayed.
    this._target.appendChild(aContents);
  },

  window: null,
  document: null,
  ownerView: null,
  _target: null
};
