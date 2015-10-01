/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource:///modules/devtools/client/shared/widgets/ViewHelpers.jsm");
Cu.import("resource://gre/modules/devtools/shared/event-emitter.js");

this.EXPORTED_SYMBOLS = ["SideMenuWidget"];

/**
 * A simple side menu, with the ability of grouping menu items.
 *
 * Note: this widget should be used in tandem with the WidgetMethods in
 * ViewHelpers.jsm.
 *
 * @param nsIDOMNode aNode
 *        The element associated with the widget.
 * @param Object aOptions
 *        - contextMenu: optional element or element ID that serves as a context menu.
 *        - showArrows: specifies if items should display horizontal arrows.
 *        - showItemCheckboxes: specifies if items should display checkboxes.
 *        - showGroupCheckboxes: specifies if groups should display checkboxes.
 */
this.SideMenuWidget = function SideMenuWidget(aNode, aOptions={}) {
  this.document = aNode.ownerDocument;
  this.window = this.document.defaultView;
  this._parent = aNode;

  let { contextMenu, showArrows, showItemCheckboxes, showGroupCheckboxes } = aOptions;
  this._contextMenu = contextMenu || null;
  this._showArrows = showArrows || false;
  this._showItemCheckboxes = showItemCheckboxes || false;
  this._showGroupCheckboxes = showGroupCheckboxes || false;

  // Create an internal scrollbox container.
  this._list = this.document.createElement("scrollbox");
  this._list.className = "side-menu-widget-container theme-sidebar";
  this._list.setAttribute("flex", "1");
  this._list.setAttribute("orient", "vertical");
  this._list.setAttribute("with-arrows", this._showArrows);
  this._list.setAttribute("with-item-checkboxes", this._showItemCheckboxes);
  this._list.setAttribute("with-group-checkboxes", this._showGroupCheckboxes);
  this._list.setAttribute("tabindex", "0");
  this._list.addEventListener("contextmenu", e => this._showContextMenu(e), false);
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
  ViewHelpers.delegateWidgetAttributeMethods(this, aNode);
  ViewHelpers.delegateWidgetEventMethods(this, aNode);
};

SideMenuWidget.prototype = {
  /**
   * Specifies if groups in this container should be sorted.
   */
  sortedGroups: true,

  /**
   * The comparator used to sort groups.
   */
  groupSortPredicate: (a, b) => a.localeCompare(b),

  /**
   * Inserts an item in this container at the specified index, optionally
   * grouping by name.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param nsIDOMNode aContents
   *        The node displayed in the container.
   * @param object aAttachment [optional]
   *        Some attached primitive/object. Custom options supported:
   *          - group: a string specifying the group to place this item into
   *          - checkboxState: the checked state of the checkbox, if shown
   *          - checkboxTooltip: the tooltip text for the checkbox, if shown
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertItemAt: function(aIndex, aContents, aAttachment={}) {
    let group = this._getMenuGroupForName(aAttachment.group);
    let item = this._getMenuItemForGroup(group, aContents, aAttachment);
    let element = item.insertSelfAt(aIndex);

    return element;
  },

  /**
   * Checks to see if the list is scrolled all the way to the bottom.
   * Uses getBoundsWithoutFlushing to limit the performance impact
   * of this function.
   *
   * @return bool
   */
  isScrolledToBottom: function() {
    if (this._list.lastElementChild) {
      let utils = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
      let childRect = utils.getBoundsWithoutFlushing(this._list.lastElementChild);
      let listRect = utils.getBoundsWithoutFlushing(this._list);

      // Cheap way to check if it's scrolled all the way to the bottom.
      return (childRect.height + childRect.top) <= listRect.bottom;
    }

    return false;
  },

  /**
   * Scroll the list to the bottom after a timeout.
   * If the user scrolls in the meantime, cancel this operation.
   */
  scrollToBottom: function() {
    this._list.scrollTop = this._list.scrollHeight;
    this.emit("scroll-to-bottom");
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
    this._getNodeForContents(aChild).remove();

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
  get selectedItem() {
    return this._selectedItem;
  },

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
        this._getNodeForContents(node).classList.add("selected");
        this._selectedItem = node;
      } else {
        this._getNodeForContents(node).classList.remove("selected");
      }
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
  _showEmptyText: function() {
    if (this._emptyTextNode || !this._emptyTextValue) {
      return;
    }
    let label = this.document.createElement("label");
    label.className = "plain side-menu-widget-empty-text";
    label.setAttribute("value", this._emptyTextValue);

    this._parent.insertBefore(label, this._list);
    this._emptyTextNode = label;
  },

  /**
   * Removes the label representing a notice in this container.
   */
  _removeEmptyText: function() {
    if (!this._emptyTextNode) {
      return;
    }

    this._parent.removeChild(this._emptyTextNode);
    this._emptyTextNode = null;
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
      showCheckbox: this._showGroupCheckboxes
    });

    this._groupsByName.set(aName, group);
    group.insertSelfAt(this.sortedGroups ? group.findExpectedIndexForSelf(this.groupSortPredicate) : -1);

    return group;
  },

  /**
   * Gets a menu item to be displayed inside a group.
   * @see SideMenuWidget.prototype._getMenuGroupForName
   *
   * @param SideMenuGroup aGroup
   *        The group to contain the menu item.
   * @param nsIDOMNode aContents
   *        The node displayed in the container.
   * @param object aAttachment [optional]
   *        Some attached primitive/object.
   */
  _getMenuItemForGroup: function(aGroup, aContents, aAttachment) {
    return new SideMenuItem(aGroup, aContents, aAttachment, {
      showArrow: this._showArrows,
      showCheckbox: this._showItemCheckboxes
    });
  },

  /**
   * Returns the .side-menu-widget-item node corresponding to a SideMenuItem.
   * To optimize the markup, some redundant elemenst are skipped when creating
   * these child items, in which case we need to be careful on which nodes
   * .selected class names are added, or which nodes are removed.
   *
   * @param nsIDOMNode aChild
   *        An element which is the target node of a SideMenuItem.
   * @return nsIDOMNode
   *         The wrapper node if there is one, or the same child otherwise.
   */
  _getNodeForContents: function(aChild) {
    if (aChild.hasAttribute("merged-item-contents")) {
      return aChild;
    } else {
      return aChild.parentNode;
    }
  },

  /**
   * Shows the contextMenu element.
   */
  _showContextMenu: function(e) {
    if (!this._contextMenu) {
      return;
    }

    // Don't show the menu if a descendant node is going to be visible also.
    let node = e.originalTarget;
    while (node && node !== this._list) {
      if (node.hasAttribute("contextmenu")) {
        return;
      }
      node = node.parentNode;
    }

    this._contextMenu.openPopupAtScreen(e.screenX, e.screenY, true);
  },

  window: null,
  document: null,
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
  _emptyTextNode: null,
  _emptyTextValue: ""
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

    let list = this._list = this.document.createElement("vbox");
    list.className = "side-menu-widget-group-list";

    let title = this._title = this.document.createElement("hbox");
    title.className = "side-menu-widget-group-title";

    let name = this._name = this.document.createElement("label");
    name.className = "plain name";
    name.setAttribute("value", aName);
    name.setAttribute("crop", "end");
    name.setAttribute("flex", "1");

    // Show a checkbox before the content.
    if (aOptions.showCheckbox) {
      let checkbox = this._checkbox = makeCheckbox(title, { description: aName });
      checkbox.className = "side-menu-widget-group-checkbox";
    }

    title.appendChild(name);
    target.appendChild(title);
    target.appendChild(list);
  }
  // Skip a few redundant nodes when no title is shown.
  else {
    let target = this._target = this._list = this.document.createElement("vbox");
    target.className = "side-menu-widget-group side-menu-widget-group-list";
    target.setAttribute("merged-group-contents", "");
  }
}

SideMenuGroup.prototype = {
  get _orderedGroupElementsArray() {
    return this.ownerView._orderedGroupElementsArray;
  },
  get _orderedMenuElementsArray() {
    return this.ownerView._orderedMenuElementsArray;
  },
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
  findExpectedIndexForSelf: function(sortPredicate) {
    let identifier = this.identifier;
    let groupsArray = this._orderedGroupElementsArray;

    for (let group of groupsArray) {
      let name = group.getAttribute("name");
      if (sortPredicate(name, identifier) > 0 && // Insertion sort at its best :)
          !name.includes(identifier)) { // Least significant group should be last.
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
 * @param nsIDOMNode aContents
 *        The node displayed in the container.
 * @param object aAttachment [optional]
 *        The attachment object.
 * @param object aOptions [optional]
 *        An object containing the following properties:
 *          - showArrow: specifies if a horizontal arrow should be displayed.
 *          - showCheckbox: specifies if a checkbox should be displayed.
 */
function SideMenuItem(aGroup, aContents, aAttachment={}, aOptions={}) {
  this.document = aGroup.document;
  this.window = aGroup.window;
  this.ownerView = aGroup;

  if (aOptions.showArrow || aOptions.showCheckbox) {
    let container = this._container = this.document.createElement("hbox");
    container.className = "side-menu-widget-item";

    let target = this._target = this.document.createElement("vbox");
    target.className = "side-menu-widget-item-contents";

    // Show a checkbox before the content.
    if (aOptions.showCheckbox) {
      let checkbox = this._checkbox = makeCheckbox(container, aAttachment);
      checkbox.className = "side-menu-widget-item-checkbox";
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
    target.setAttribute("merged-item-contents", "");
  }

  this._target.setAttribute("flex", "1");
  this.contents = aContents;
}

SideMenuItem.prototype = {
  get _orderedGroupElementsArray() {
    return this.ownerView._orderedGroupElementsArray;
  },
  get _orderedMenuElementsArray() {
    return this.ownerView._orderedMenuElementsArray;
  },
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
