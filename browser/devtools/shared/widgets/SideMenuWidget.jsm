/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
  "resource://gre/modules/devtools/Loader.jsm");

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return devtools.require("devtools/toolkit/webconsole/network-helper");
  },
  configurable: true,
  enumerable: true
});

this.EXPORTED_SYMBOLS = ["SideMenuWidget"];

/**
 * A simple side menu, with the ability of grouping menu items.
 * This widget should be used in tandem with the WidgetMethods in ViewHelpers.jsm
 *
 * @param nsIDOMNode aNode
 *        The element associated with the widget.
 * @param Object aOptions
 *        - theme: "light" or "dark", defaults to dark if falsy.
 *        - showArrows: specifies if items should display horizontal arrows.
 *        - showItemCheckboxes: specifies if items should display checkboxes.
 *        - showGroupCheckboxes: specifies if groups should display checkboxes.
 */
this.SideMenuWidget = function SideMenuWidget(aNode, aOptions={}) {
  this.document = aNode.ownerDocument;
  this.window = this.document.defaultView;
  this._parent = aNode;

  let { theme, showArrows, showItemCheckboxes, showGroupCheckboxes } = aOptions;
  this._theme = theme || "dark";
  this._showArrows = showArrows || false;
  this._showItemCheckboxes = showItemCheckboxes || false;
  this._showGroupCheckboxes = showGroupCheckboxes || false;

  // Create an internal scrollbox container.
  this._list = this.document.createElement("scrollbox");
  this._list.className = "side-menu-widget-container";
  this._list.setAttribute("flex", "1");
  this._list.setAttribute("orient", "vertical");
  this._list.setAttribute("theme", this._theme);
  this._list.setAttribute("with-arrows", this._showArrows);
  this._list.setAttribute("with-item-checkboxes", this._showItemCheckboxes);
  this._list.setAttribute("with-group-checkboxes", this._showGroupCheckboxes);
  this._list.setAttribute("tabindex", "0");
  this._list.addEventListener("keypress", e => this.emit("keyPress", e), false);
  this._list.addEventListener("mousedown", e => this.emit("mousePress", e), false);
  this._parent.appendChild(this._list);

  // Menu items can optionally be grouped.
  this._groupsByName = new Map(); // Can't use a WeakMap because keys are strings.
  this._orderedGroupElementsArray = [];
  this._orderedMenuElementsArray = [];
  this._itemsByElement = new Map();

  // This widget emits events that can be handled in a MenuContainer.
  EventEmitter.decorate(this);

  // Delegate some of the associated node's methods to satisfy the interface
  // required by MenuContainer instances.
  ViewHelpers.delegateWidgetEventMethods(this, aNode);
};

SideMenuWidget.prototype = {
  /**
   * Specifies if groups in this container should be sorted alphabetically.
   */
  sortedGroups: true,

  /**
   * Specifies if this container should try to keep the selected item visible.
   * (For example, when new items are added the selection is brought into view).
   */
  maintainSelectionVisible: true,

  /**
   * Specifies that the container viewport should be "stuck" to the
   * bottom. That is, the container is automatically scrolled down to
   * keep appended items visible, but only when the scroll position is
   * already at the bottom.
   */
  autoscrollWithAppendedItems: false,

  /**
   * Inserts an item in this container at the specified index, optionally
   * grouping by name.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param string | nsIDOMNode aContents
   *        The string or node displayed in the container.
   * @param string aTooltip [optional]
   *        A tooltip attribute for the displayed item.
   * @param string aGroup [optional]
   *        The group to place the displayed item into.
   * @param Object aAttachment [optional]
   *        Extra data for the user.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertItemAt: function(aIndex, aContents, aTooltip = "", aGroup = "", aAttachment={}) {
    aTooltip = NetworkHelper.convertToUnicode(unescape(aTooltip));
    aGroup = NetworkHelper.convertToUnicode(unescape(aGroup));

    // Invalidate any notices set on this widget.
    this.removeAttribute("notice");

    // Maintaining scroll position at the bottom when a new item is inserted
    // depends on several factors (the order of testing is important to avoid
    // needlessly expensive operations that may cause reflows):
    let maintainScrollAtBottom =
      // 1. The behavior should be enabled,
      this.autoscrollWithAppendedItems &&
      // 2. There shouldn't currently be any selected item in the list.
      !this._selectedItem &&
      // 3. The new item should be appended at the end of the list.
      (aIndex < 0 || aIndex >= this._orderedMenuElementsArray.length) &&
      // 4. The list should already be scrolled at the bottom.
      (this._list.scrollTop + this._list.clientHeight >= this._list.scrollHeight);

    let group = this._getMenuGroupForName(aGroup);
    let item = this._getMenuItemForGroup(group, aContents, aTooltip, aAttachment);
    let element = item.insertSelfAt(aIndex);

    if (this.maintainSelectionVisible) {
      this.ensureElementIsVisible(this.selectedItem);
    }
    if (maintainScrollAtBottom) {
      this._list.scrollTop = this._list.scrollHeight;
    }

    return element;
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
    return this._orderedMenuElementsArray[aIndex];
  },

  /**
   * Removes the specified child node from this container.
   *
   * @param nsIDOMNode aChild
   *        The element associated with the displayed item.
   */
  removeChild: function(aChild) {
    if (aChild.classList.contains("side-menu-widget-item-contents") &&
       !aChild.classList.contains("side-menu-widget-item")) {
      // Remove the item itself, not the contents.
      aChild.parentNode.remove();
    } else {
      // Groups with no title don't have any special internal structure.
      aChild.remove();
    }

    this._orderedMenuElementsArray.splice(
      this._orderedMenuElementsArray.indexOf(aChild), 1);
    this._itemsByElement.delete(aChild);

    if (this._selectedItem == aChild) {
      this._selectedItem = null;
    }
  },

  /**
   * Removes all of the child nodes from this container.
   */
  removeAllItems: function() {
    let parent = this._parent;
    let list = this._list;

    while (list.hasChildNodes()) {
      list.firstChild.remove();
    }

    this._selectedItem = null;

    this._groupsByName.clear();
    this._orderedGroupElementsArray.length = 0;
    this._orderedMenuElementsArray.length = 0;
    this._itemsByElement.clear();
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
    let menuArray = this._orderedMenuElementsArray;

    if (!aChild) {
      this._selectedItem = null;
    }
    for (let node of menuArray) {
      if (node == aChild) {
        node.classList.add("selected");
        node.parentNode.classList.add("selected");
        this._selectedItem = node;
      } else {
        node.classList.remove("selected");
        node.parentNode.classList.remove("selected");
      }
    }

    this.ensureElementIsVisible(this.selectedItem);
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
    let boxObject = this._list.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    boxObject.ensureElementIsVisible(aElement);
    boxObject.scrollBy(-aElement.clientWidth, 0);
  },

  /**
   * Shows all the groups, even the ones with no visible children.
   */
  showEmptyGroups: function() {
    for (let group of this._orderedGroupElementsArray) {
      group.hidden = false;
    }
  },

  /**
   * Hides all the groups which have no visible children.
   */
  hideEmptyGroups: function() {
    let visibleChildNodes = ".side-menu-widget-item-contents:not([hidden=true])";

    for (let group of this._orderedGroupElementsArray) {
      group.hidden = group.querySelectorAll(visibleChildNodes).length == 0;
    }
    for (let menuItem of this._orderedMenuElementsArray) {
      menuItem.parentNode.hidden = menuItem.hidden;
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
    return this._parent.getAttribute(aName);
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

    if (aName == "notice") {
      this.notice = aValue;
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

    if (aName == "notice") {
      this._removeNotice();
    }
  },

  /**
   * Set the checkbox state for the item associated with the given node.
   *
   * @param nsIDOMNode aNode
   *        The dom node for an item we want to check.
   * @param boolean aCheckState
   *        True to check, false to uncheck.
   */
  checkItem: function(aNode, aCheckState) {
    const widgetItem = this._itemsByElement.get(aNode);
    if (!widgetItem) {
      throw new Error("No item for " + aNode);
    }
    widgetItem.check(aCheckState);
  },

  /**
   * Sets the text displayed in this container as a notice.
   * @param string aValue
   */
  set notice(aValue) {
    if (this._noticeTextNode) {
      this._noticeTextNode.setAttribute("value", aValue);
    }
    this._noticeTextValue = aValue;
    this._appendNotice();
  },

  /**
   * Creates and appends a label representing a notice in this container.
   */
  _appendNotice: function() {
    if (this._noticeTextNode || !this._noticeTextValue) {
      return;
    }

    let container = this.document.createElement("vbox");
    container.className = "side-menu-widget-empty-notice-container";
    container.setAttribute("theme", this._theme);

    let label = this.document.createElement("label");
    label.className = "plain side-menu-widget-empty-notice";
    label.setAttribute("value", this._noticeTextValue);
    container.appendChild(label);

    this._parent.insertBefore(container, this._list);
    this._noticeTextContainer = container;
    this._noticeTextNode = label;
  },

  /**
   * Removes the label representing a notice in this container.
   */
  _removeNotice: function() {
    if (!this._noticeTextNode) {
      return;
    }

    this._parent.removeChild(this._noticeTextContainer);
    this._noticeTextContainer = null;
    this._noticeTextNode = null;
  },

  /**
   * Gets a container representing a group for menu items. If the container
   * is not available yet, it is immediately created.
   *
   * @param string aName
   *        The required group name.
   * @return SideMenuGroup
   *         The newly created group.
   */
  _getMenuGroupForName: function(aName) {
    let cachedGroup = this._groupsByName.get(aName);
    if (cachedGroup) {
      return cachedGroup;
    }

    let group = new SideMenuGroup(this, aName, {
      theme: this._theme,
      showCheckbox: this._showGroupCheckboxes
    });

    this._groupsByName.set(aName, group);
    group.insertSelfAt(this.sortedGroups ? group.findExpectedIndexForSelf() : -1);

    return group;
  },

  /**
   * Gets a menu item to be displayed inside a group.
   * @see SideMenuWidget.prototype._getMenuGroupForName
   *
   * @param SideMenuGroup aGroup
   *        The group to contain the menu item.
   * @param string | nsIDOMNode aContents
   *        The string or node displayed in the container.
   * @param string aTooltip [optional]
   *        A tooltip attribute for the displayed item.
   * @param object aAttachment [optional]
   *        The attachement object.
   */
  _getMenuItemForGroup: function(aGroup, aContents, aTooltip, aAttachment) {
    return new SideMenuItem(aGroup, aContents, aTooltip, aAttachment, {
      theme: this._theme,
      showArrow: this._showArrows,
      showCheckbox: this._showItemCheckboxes
    });
  },

  window: null,
  document: null,
  _theme: "",
  _showArrows: false,
  _showItemCheckboxes: false,
  _showGroupCheckboxes: false,
  _parent: null,
  _list: null,
  _selectedItem: null,
  _groupsByName: null,
  _orderedGroupElementsArray: null,
  _orderedMenuElementsArray: null,
  _itemsByElement: null,
  _ensureVisibleTimeout: null,
  _noticeTextContainer: null,
  _noticeTextNode: null,
  _noticeTextValue: ""
};

/**
 * A SideMenuGroup constructor for the BreadcrumbsWidget.
 * Represents a group which should contain SideMenuItems.
 *
 * @param SideMenuWidget aWidget
 *        The widget to contain this menu item.
 * @param string aName
 *        The string displayed in the container.
 * @param object aOptions [optional]
 *        An object containing the following properties:
 *          - theme: the theme colors, either "dark" or "light".
 *          - showCheckbox: specifies if a checkbox should be displayed.
 */
function SideMenuGroup(aWidget, aName, aOptions={}) {
  this.document = aWidget.document;
  this.window = aWidget.window;
  this.ownerView = aWidget;
  this.identifier = aName;

  // Create an internal title and list container.
  if (aName) {
    let target = this._target = this.document.createElement("vbox");
    target.className = "side-menu-widget-group";
    target.setAttribute("name", aName);
    target.setAttribute("tooltiptext", aName);

    let list = this._list = this.document.createElement("vbox");
    list.className = "side-menu-widget-group-list";

    let title = this._title = this.document.createElement("hbox");
    title.className = "side-menu-widget-group-title";
    title.setAttribute("theme", aOptions.theme);

    let name = this._name = this.document.createElement("label");
    name.className = "plain name";
    name.setAttribute("value", aName);
    name.setAttribute("crop", "end");
    name.setAttribute("flex", "1");

    // Show a checkbox before the content.
    if (aOptions.showCheckbox) {
      let checkbox = this._checkbox = makeCheckbox(title, { description: aName });
      checkbox.className = "side-menu-widget-group-checkbox";
      checkbox.setAttribute("align", "start");
    }

    title.appendChild(name);
    target.appendChild(title);
    target.appendChild(list);
  }
  // Skip a few redundant nodes when no title is shown.
  else {
    let target = this._target = this._list = this.document.createElement("vbox");
    target.className = "side-menu-widget-group side-menu-widget-group-list";
    target.setAttribute("theme", aOptions.theme);
  }
}

SideMenuGroup.prototype = {
  get _orderedGroupElementsArray() this.ownerView._orderedGroupElementsArray,
  get _orderedMenuElementsArray() this.ownerView._orderedMenuElementsArray,
  get _itemsByElement() { return this.ownerView._itemsByElement; },

  /**
   * Inserts this group in the parent container at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this group.
   */
  insertSelfAt: function(aIndex) {
    let ownerList = this.ownerView._list;
    let groupsArray = this._orderedGroupElementsArray;

    if (aIndex >= 0) {
      ownerList.insertBefore(this._target, groupsArray[aIndex]);
      groupsArray.splice(aIndex, 0, this._target);
    } else {
      ownerList.appendChild(this._target);
      groupsArray.push(this._target);
    }
  },

  /**
   * Finds the expected index of this group based on its name.
   *
   * @return number
   *         The expected index.
   */
  findExpectedIndexForSelf: function() {
    let identifier = this.identifier;
    let groupsArray = this._orderedGroupElementsArray;

    for (let group of groupsArray) {
      let name = group.getAttribute("name");
      if (name > identifier && // Insertion sort at its best :)
         !name.contains(identifier)) { // Least significat group should be last.
        return groupsArray.indexOf(group);
      }
    }
    return -1;
  },

  window: null,
  document: null,
  ownerView: null,
  identifier: "",
  _target: null,
  _checkbox: null,
  _title: null,
  _name: null,
  _list: null
};

/**
 * A SideMenuItem constructor for the BreadcrumbsWidget.
 *
 * @param SideMenuGroup aGroup
 *        The group to contain this menu item.
 * @param string aTooltip [optional]
 *        A tooltip attribute for the displayed item.
 * @param string | nsIDOMNode aContents
 *        The string or node displayed in the container.
 * @param object aAttachment [optional]
 *        The attachment object.
 * @param object aOptions [optional]
 *        An object containing the following properties:
 *          - theme: the theme colors, either "dark" or "light".
 *          - showArrow: specifies if a horizontal arrow should be displayed.
 *          - showCheckbox: specifies if a checkbox should be displayed.
 */
function SideMenuItem(aGroup, aContents, aTooltip, aAttachment={}, aOptions={}) {
  this.document = aGroup.document;
  this.window = aGroup.window;
  this.ownerView = aGroup;

  if (aOptions.showArrow || aOptions.showCheckbox) {
    let container = this._container = this.document.createElement("hbox");
    container.className = "side-menu-widget-item";
    container.setAttribute("tooltiptext", aTooltip);
    container.setAttribute("theme", aOptions.theme);

    let target = this._target = this.document.createElement("vbox");
    target.className = "side-menu-widget-item-contents";

    // Show a checkbox before the content.
    if (aOptions.showCheckbox) {
      let checkbox = this._checkbox = makeCheckbox(container, aAttachment);
      checkbox.className = "side-menu-widget-item-checkbox";
      checkbox.setAttribute("align", "start");
    }

    container.appendChild(target);

    // Show a horizontal arrow towards the content.
    if (aOptions.showArrow) {
      let arrow = this._arrow = this.document.createElement("hbox");
      arrow.className = "side-menu-widget-item-arrow";
      container.appendChild(arrow);
    }
  }
  // Skip a few redundant nodes when no horizontal arrow or checkbox is shown.
  else {
    let target = this._target = this._container = this.document.createElement("hbox");
    target.className = "side-menu-widget-item side-menu-widget-item-contents";
    target.setAttribute("theme", aOptions.theme);
  }

  this._target.setAttribute("flex", "1");
  this.contents = aContents;
}

SideMenuItem.prototype = {
  get _orderedGroupElementsArray() this.ownerView._orderedGroupElementsArray,
  get _orderedMenuElementsArray() this.ownerView._orderedMenuElementsArray,
  get _itemsByElement() { return this.ownerView._itemsByElement; },

  /**
   * Inserts this item in the parent group at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertSelfAt: function(aIndex) {
    let ownerList = this.ownerView._list;
    let menuArray = this._orderedMenuElementsArray;

    if (aIndex >= 0) {
      ownerList.insertBefore(this._container, ownerList.childNodes[aIndex]);
      menuArray.splice(aIndex, 0, this._target);
    } else {
      ownerList.appendChild(this._container);
      menuArray.push(this._target);
    }
    this._itemsByElement.set(this._target, this);

    return this._target;
  },

  /**
   * Check or uncheck the checkbox associated with this item.
   *
   * @param boolean aCheckState
   *        True to check, false to uncheck.
   */
  check: function(aCheckState) {
    if (!this._checkbox) {
      throw new Error("Cannot check items that do not have checkboxes.");
    }
    // Don't set or remove the "checked" attribute, assign the property instead.
    // Otherwise, the "CheckboxStateChange" event will not be fired. XUL!!
    this._checkbox.checked = !!aCheckState;
  },

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
      label.className = "side-menu-widget-item-label";
      label.setAttribute("value", aContents);
      label.setAttribute("crop", "start");
      label.setAttribute("flex", "1");
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

  window: null,
  document: null,
  ownerView: null,
  _target: null,
  _container: null,
  _checkbox: null,
  _arrow: null
};

/**
 * Creates a checkbox to a specified parent node. Emits a "check" event
 * whenever the checkbox is checked or unchecked by the user.
 *
 * @param nsIDOMNode aParentNode
 *        The parent node to contain this checkbox.
 * @param object aOptions
 *        An object containing some or all of the following properties:
 *          - description: defaults to "item" if unspecified
 *          - checkboxState: true for checked, false for unchecked
 *          - checkboxTooltip: the tooltip text of the checkbox
 */
function makeCheckbox(aParentNode, aOptions) {
  let checkbox = aParentNode.ownerDocument.createElement("checkbox");
  checkbox.setAttribute("tooltiptext", aOptions.checkboxTooltip);

  if (aOptions.checkboxState) {
    checkbox.setAttribute("checked", true);
  } else {
    checkbox.removeAttribute("checked");
  }

  // Stop the toggling of the checkbox from selecting the list item.
  checkbox.addEventListener("mousedown", e => {
    e.stopPropagation();
  }, false);

  // Emit an event from the checkbox when it is toggled. Don't listen for the
  // "command" event! It won't fire for programmatic changes. XUL!!
  checkbox.addEventListener("CheckboxStateChange", e => {
    ViewHelpers.dispatchEvent(checkbox, "check", {
      description: aOptions.description || "item",
      checked: checkbox.checked
    });
  }, false);

  aParentNode.appendChild(checkbox);
  return checkbox;
}
