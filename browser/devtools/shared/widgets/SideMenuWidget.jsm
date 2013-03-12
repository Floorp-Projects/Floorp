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

this.EXPORTED_SYMBOLS = ["SideMenuWidget"];

/**
 * A simple side menu, with the ability of grouping menu items.
 *
 * You can use this widget alone, but it works great with a MenuContainer!
 * In that case, you should never need to access the methods in the
 * SideMenuWidget directly, use the wrapper MenuContainer instance instead.
 *
 * @see ViewHelpers.jsm
 *
 * function MyView() {
 *   this.node = new SideMenuWidget(document.querySelector(".my-node"));
 * }
 * ViewHelpers.create({ constructor: MyView, proto: MenuContainer.prototype }, {
 *   myMethod: function() {},
 *   ...
 * });
 *
 * @param nsIDOMNode aNode
 *        The element associated with the widget.
 */
this.SideMenuWidget = function SideMenuWidget(aNode) {
  this._parent = aNode;

  // Create an internal scrollbox container.
  this._list = this.document.createElement("scrollbox");
  this._list.className = "side-menu-widget-container";
  this._list.setAttribute("flex", "1");
  this._list.setAttribute("orient", "vertical");
  this._parent.appendChild(this._list);
  this._boxObject = this._list.boxObject.QueryInterface(Ci.nsIScrollBoxObject);

  // Menu items can optionally be grouped.
  this._groupsByName = new Map(); // Can't use a WeakMap because keys are strings.
  this._orderedGroupElementsArray = [];
  this._orderedMenuElementsArray = [];

  // Delegate some of the associated node's methods to satisfy the interface
  // required by MenuContainer instances.
  ViewHelpers.delegateWidgetEventMethods(this, aNode);
};

SideMenuWidget.prototype = {
  get document() this._parent.ownerDocument,
  get window() this.document.defaultView,

  /**
   * Specifies if groups in this container should be sorted alphabetically.
   */
  sortedGroups: true,

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
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertItemAt: function SMW_insertItemAt(aIndex, aContents, aTooltip = "", aGroup = "") {
    this.ensureSelectionIsVisible(true, true); // Don't worry, it's delayed.

    let group = this._getGroupForName(aGroup);
    return group.insertItemAt(aIndex, aContents, aTooltip);
  },

  /**
   * Returns the child node in this container situated at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  getItemAtIndex: function SMW_getItemAtIndex(aIndex) {
    return this._orderedMenuElementsArray[aIndex];
  },

  /**
   * Removes the specified child node from this container.
   *
   * @param nsIDOMNode aChild
   *        The element associated with the displayed item.
   */
  removeChild: function SMW_removeChild(aChild) {
    aChild.parentNode.removeChild(aChild);
    this._orderedMenuElementsArray.splice(
      this._orderedMenuElementsArray.indexOf(aChild), 1);

    if (this._selectedItem == aChild) {
      this._selectedItem = null;
    }
  },

  /**
   * Removes all of the child nodes from this container.
   */
  removeAllItems: function SMW_removeAllItems() {
    let parent = this._parent;
    let list = this._list;
    let firstChild;

    while (firstChild = list.firstChild) {
      list.removeChild(firstChild);
    }
    this._selectedItem = null;

    this._groupsByName.clear();
    this._orderedGroupElementsArray.length = 0;
    this._orderedMenuElementsArray.length = 0;
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
    let menuElementsArray = this._orderedMenuElementsArray;

    if (!aChild) {
      this._selectedItem = null;
    }
    for (let node of menuElementsArray) {
      if (node == aChild) {
        node.parentNode.classList.add("selected");
        this._selectedItem = node;
      } else {
        node.parentNode.classList.remove("selected");
      }
    }
    // Repeated calls to ensureElementIsVisible would interfere with each other
    // and may sometimes result in incorrect scroll positions.
    this.ensureSelectionIsVisible(false, true);
  },

  /**
   * Ensures the selected element is visible.
   * @see SideMenuWidget.prototype.ensureElementIsVisible.
   */
  ensureSelectionIsVisible:
  function SMW_ensureSelectionIsVisible(aGroupFlag, aDelayedFlag) {
    this.ensureElementIsVisible(this.selectedItem, aGroupFlag, aDelayedFlag);
  },

  /**
   * Ensures the specified element is visible.
   *
   * @param nsIDOMNode aElement
   *        The element to make visible.
   * @param boolean aGroupFlag
   *        True if the group header should also be made visible, if possible.
   * @param boolean aDelayedFlag
   *        True to wait a few cycles before ensuring the selection is visible.
   */
  ensureElementIsVisible:
  function SMW_ensureElementIsVisible(aElement, aGroupFlag, aDelayedFlag) {
    if (!aElement) {
      return;
    }
    if (aDelayedFlag) {
      this.window.clearTimeout(this._ensureVisibleTimeout);
      this._ensureVisibleTimeout = this.window.setTimeout(function() {
        this.ensureElementIsVisible(aElement, aGroupFlag, false);
      }.bind(this), ENSURE_SELECTION_VISIBLE_DELAY);
      return;
    }
    if (aGroupFlag) {
      let groupList = aElement.parentNode;
      let groupContainer = groupList.parentNode;
      groupContainer.scrollIntoView(true); // Align with the top.
    }
    this._boxObject.ensureElementIsVisible(aElement);
  },

  /**
   * Shows all the groups, even the ones with no visible children.
   */
  showEmptyGroups: function SMW_showEmptyGroups() {
    for (let group of this._orderedGroupElementsArray) {
      group.hidden = false;
    }
  },

  /**
   * Hides all the groups which have no visible children.
   */
  hideEmptyGroups: function SMW_hideEmptyGroups() {
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
  getAttribute: function SMW_getAttribute(aName) {
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
  setAttribute: function SMW_setAttribute(aName, aValue) {
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
  removeAttribute: function SMW_removeAttribute(aName) {
    this._parent.removeAttribute(aName);

    if (aName == "notice") {
      this._removeNotice();
    }
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
  _appendNotice: function DVSL__appendNotice() {
    if (this._noticeTextNode || !this._noticeTextValue) {
      return;
    }

    let container = this.document.createElement("vbox");
    container.className = "side-menu-widget-empty-notice-container";
    container.setAttribute("align", "center");

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
  _removeNotice: function DVSL__removeNotice() {
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
  _getGroupForName: function SMW__getGroupForName(aName) {
    let cachedGroup = this._groupsByName.get(aName);
    if (cachedGroup) {
      return cachedGroup;
    }

    let group = new SideMenuGroup(this, aName);
    this._groupsByName.set(aName, group);
    group.insertSelfAt(this.sortedGroups ? group.findExpectedIndexForSelf() : -1);
    return group;
  },

  _parent: null,
  _list: null,
  _boxObject: null,
  _selectedItem: null,
  _groupsByName: null,
  _orderedGroupElementsArray: null,
  _orderedMenuElementsArray: null,
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
 */
function SideMenuGroup(aWidget, aName) {
  this.ownerView = aWidget;
  this.identifier = aName;

  let document = this.document;
  let title = this._title = document.createElement("hbox");
  title.className = "side-menu-widget-group-title";

  let name = this._name = document.createElement("label");
  name.className = "plain name";
  name.setAttribute("value", aName);
  name.setAttribute("crop", "end");
  name.setAttribute("flex", "1");

  let list = this._list = document.createElement("vbox");
  list.className = "side-menu-widget-group-list";

  let target = this._target = document.createElement("vbox");
  target.className = "side-menu-widget-group side-menu-widget-item-or-group";
  target.setAttribute("name", aName);
  target.setAttribute("tooltiptext", aName);

  title.appendChild(name);
  target.appendChild(title);
  target.appendChild(list);
}

SideMenuGroup.prototype = {
  get document() this.ownerView.document,
  get window() this.document.defaultView,
  get _groupElementsArray() this.ownerView._orderedGroupElementsArray,
  get _menuElementsArray() this.ownerView._orderedMenuElementsArray,

  /**
   * Inserts an item in this group at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param string | nsIDOMNode aContents
   *        The string or node displayed in the container.
   * @param string aTooltip [optional]
   *        A tooltip attribute for the displayed item.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertItemAt: function SMG_insertItemAt(aIndex, aContents, aTooltip) {
    let list = this._list;
    let menuArray = this._menuElementsArray;
    let item = new SideMenuItem(this, aContents, aTooltip);

    // Invalidate any notices set on the owner widget.
    this.ownerView.removeAttribute("notice");

    if (aIndex >= 0) {
      list.insertBefore(item._container, list.childNodes[aIndex]);
      menuArray.splice(aIndex, 0, item._target);
    } else {
      list.appendChild(item._container);
      menuArray.push(item._target);
    }

    return item._target;
  },

  /**
   * Inserts this group in the parent container at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this group.
   */
  insertSelfAt: function SMG_insertSelfAt(aIndex) {
    let ownerList = this.ownerView._list;
    let groupsArray = this._groupElementsArray;

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
  findExpectedIndexForSelf: function SMG_findExpectedIndexForSelf() {
    let identifier = this.identifier;
    let groupsArray = this._groupElementsArray;

    for (let group of groupsArray) {
      let name = group.getAttribute("name");
      if (name > identifier && // Insertion sort at its best :)
         !name.contains(identifier)) { // Least significat group should be last.
        return groupsArray.indexOf(group);
      }
    }
    return -1;
  },

  ownerView: null,
  identifier: "",
  _target: null,
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
 */
function SideMenuItem(aGroup, aContents, aTooltip = "") {
  this.ownerView = aGroup;

  let document = this.document;
  let target = this._target = document.createElement("vbox");
  target.className = "side-menu-widget-item-contents";
  target.setAttribute("flex", "1");
  this.contents = aContents;

  let arrow = this._arrow = document.createElement("hbox");
  arrow.className = "side-menu-widget-item-arrow";

  let container = this._container = document.createElement("hbox");
  container.className = "side-menu-widget-item side-menu-widget-item-or-group";
  container.setAttribute("tooltiptext", aTooltip);
  container.appendChild(target);
  container.appendChild(arrow);
}

SideMenuItem.prototype = {
  get document() this.ownerView.document,
  get window() this.document.defaultView,

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

  ownerView: null,
  _target: null,
  _container: null,
  _arrow: null
};
