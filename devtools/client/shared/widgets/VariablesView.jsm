/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const DBG_STRINGS_URI = "devtools/client/locales/debugger.properties";
const LAZY_EMPTY_DELAY = 150; // ms
const SCROLL_PAGE_SIZE_DEFAULT = 0;
const PAGE_SIZE_SCROLL_HEIGHT_RATIO = 100;
const PAGE_SIZE_MAX_JUMPS = 30;
const SEARCH_ACTION_MAX_DELAY = 300; // ms
const ITEM_FLASH_DURATION = 300; // ms

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const EventEmitter = require("devtools/shared/event-emitter");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const Services = require("Services");
const { getSourceNames } = require("devtools/client/shared/source-utils");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const { extend } = require("devtools/shared/extend");
const { ViewHelpers, setNamedTimeout } =
  require("devtools/client/shared/widgets/view-helpers");
const nodeConstants = require("devtools/shared/dom-node-constants");
const {KeyCodes} = require("devtools/client/shared/keycodes");
const {PluralForm} = require("devtools/shared/plural-form");
const {LocalizationHelper, ELLIPSIS} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(DBG_STRINGS_URI);

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper");

Object.defineProperty(this, "WebConsoleUtils", {
  get: function() {
    return require("devtools/client/webconsole/utils").Utils;
  },
  configurable: true,
  enumerable: true
});

this.EXPORTED_SYMBOLS = ["VariablesView", "escapeHTML"];

/**
 * A tree view for inspecting scopes, objects and properties.
 * Iterable via "for (let [id, scope] of instance) { }".
 * Requires the devtools common.css and debugger.css skin stylesheets.
 *
 * To allow replacing variable or property values in this view, provide an
 * "eval" function property. To allow replacing variable or property names,
 * provide a "switch" function. To handle deleting variables or properties,
 * provide a "delete" function.
 *
 * @param nsIDOMNode aParentNode
 *        The parent node to hold this view.
 * @param object aFlags [optional]
 *        An object contaning initialization options for this view.
 *        e.g. { lazyEmpty: true, searchEnabled: true ... }
 */
this.VariablesView = function VariablesView(aParentNode, aFlags = {}) {
  this._store = []; // Can't use a Map because Scope names needn't be unique.
  this._itemsByElement = new WeakMap();
  this._prevHierarchy = new Map();
  this._currHierarchy = new Map();

  this._parent = aParentNode;
  this._parent.classList.add("variables-view-container");
  this._parent.classList.add("theme-body");
  this._appendEmptyNotice();

  this._onSearchboxInput = this._onSearchboxInput.bind(this);
  this._onSearchboxKeyDown = this._onSearchboxKeyDown.bind(this);
  this._onViewKeyDown = this._onViewKeyDown.bind(this);

  // Create an internal scrollbox container.
  this._list = this.document.createElement("scrollbox");
  this._list.setAttribute("orient", "vertical");
  this._list.addEventListener("keydown", this._onViewKeyDown);
  this._parent.appendChild(this._list);

  for (let name in aFlags) {
    this[name] = aFlags[name];
  }

  EventEmitter.decorate(this);
};

VariablesView.prototype = {
  /**
   * Helper setter for populating this container with a raw object.
   *
   * @param object aObject
   *        The raw object to display. You can only provide this object
   *        if you want the variables view to work in sync mode.
   */
  set rawObject(aObject) {
    this.empty();
    this.addScope()
        .addItem(undefined, { enumerable: true })
        .populate(aObject, { sorted: true });
  },

  /**
   * Adds a scope to contain any inspected variables.
   *
   * This new scope will be considered the parent of any other scope
   * added afterwards.
   *
   * @param string aName
   *        The scope's name (e.g. "Local", "Global" etc.).
   * @param string aCustomClass
   *        An additional class name for the containing element.
   * @return Scope
   *         The newly created Scope instance.
   */
  addScope: function(aName = "", aCustomClass = "") {
    this._removeEmptyNotice();
    this._toggleSearchVisibility(true);

    let scope = new Scope(this, aName, { customClass: aCustomClass });
    this._store.push(scope);
    this._itemsByElement.set(scope._target, scope);
    this._currHierarchy.set(aName, scope);
    scope.header = !!aName;

    return scope;
  },

  /**
   * Removes all items from this container.
   *
   * @param number aTimeout [optional]
   *        The number of milliseconds to delay the operation if
   *        lazy emptying of this container is enabled.
   */
  empty: function(aTimeout = this.lazyEmptyDelay) {
    // If there are no items in this container, emptying is useless.
    if (!this._store.length) {
      return;
    }

    this._store.length = 0;
    this._itemsByElement = new WeakMap();
    this._prevHierarchy = this._currHierarchy;
    this._currHierarchy = new Map(); // Don't clear, this is just simple swapping.

    // Check if this empty operation may be executed lazily.
    if (this.lazyEmpty && aTimeout > 0) {
      this._emptySoon(aTimeout);
      return;
    }

    while (this._list.hasChildNodes()) {
      this._list.firstChild.remove();
    }

    this._appendEmptyNotice();
    this._toggleSearchVisibility(false);
  },

  /**
   * Emptying this container and rebuilding it immediately afterwards would
   * result in a brief redraw flicker, because the previously expanded nodes
   * may get asynchronously re-expanded, after fetching the prototype and
   * properties from a server.
   *
   * To avoid such behaviour, a normal container list is rebuild, but not
   * immediately attached to the parent container. The old container list
   * is kept around for a short period of time, hopefully accounting for the
   * data fetching delay. In the meantime, any operations can be executed
   * normally.
   *
   * @see VariablesView.empty
   * @see VariablesView.commitHierarchy
   */
  _emptySoon: function(aTimeout) {
    let prevList = this._list;
    let currList = this._list = this.document.createElement("scrollbox");

    this.window.setTimeout(() => {
      prevList.removeEventListener("keydown", this._onViewKeyDown);
      currList.addEventListener("keydown", this._onViewKeyDown);
      currList.setAttribute("orient", "vertical");

      this._parent.removeChild(prevList);
      this._parent.appendChild(currList);

      if (!this._store.length) {
        this._appendEmptyNotice();
        this._toggleSearchVisibility(false);
      }
    }, aTimeout);
  },

  /**
   * Optional DevTools toolbox containing this VariablesView. Used to
   * communicate with the inspector and highlighter.
   */
  toolbox: null,

  /**
   * The controller for this VariablesView, if it has one.
   */
  controller: null,

  /**
   * The amount of time (in milliseconds) it takes to empty this view lazily.
   */
  lazyEmptyDelay: LAZY_EMPTY_DELAY,

  /**
   * Specifies if this view may be emptied lazily.
   * @see VariablesView.prototype.empty
   */
  lazyEmpty: false,

  /**
   * Specifies if nodes in this view may be searched lazily.
   */
  lazySearch: true,

  /**
   * The number of elements in this container to jump when Page Up or Page Down
   * keys are pressed. If falsy, then the page size will be based on the
   * container height.
   */
  scrollPageSize: SCROLL_PAGE_SIZE_DEFAULT,

  /**
   * Function called each time a variable or property's value is changed via
   * user interaction. If null, then value changes are disabled.
   *
   * This property is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  eval: null,

  /**
   * Function called each time a variable or property's name is changed via
   * user interaction. If null, then name changes are disabled.
   *
   * This property is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  switch: null,

  /**
   * Function called each time a variable or property is deleted via
   * user interaction. If null, then deletions are disabled.
   *
   * This property is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  delete: null,

  /**
   * Function called each time a property is added via user interaction. If
   * null, then property additions are disabled.
   *
   * This property is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  new: null,

  /**
   * Specifies if after an eval or switch operation, the variable or property
   * which has been edited should be disabled.
   */
  preventDisableOnChange: false,

  /**
   * Specifies if, whenever a variable or property descriptor is available,
   * configurable, enumerable, writable, frozen, sealed and extensible
   * attributes should not affect presentation.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  preventDescriptorModifiers: false,

  /**
   * The tooltip text shown on a variable or property's value if an |eval|
   * function is provided, in order to change the variable or property's value.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  editableValueTooltip: L10N.getStr("variablesEditableValueTooltip"),

  /**
   * The tooltip text shown on a variable or property's name if a |switch|
   * function is provided, in order to change the variable or property's name.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  editableNameTooltip: L10N.getStr("variablesEditableNameTooltip"),

  /**
   * The tooltip text shown on a variable or property's edit button if an
   * |eval| function is provided and a getter/setter descriptor is present,
   * in order to change the variable or property to a plain value.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  editButtonTooltip: L10N.getStr("variablesEditButtonTooltip"),

  /**
   * The tooltip text shown on a variable or property's value if that value is
   * a DOMNode that can be highlighted and selected in the inspector.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  domNodeValueTooltip: L10N.getStr("variablesDomNodeValueTooltip"),

  /**
   * The tooltip text shown on a variable or property's delete button if a
   * |delete| function is provided, in order to delete the variable or property.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  deleteButtonTooltip: L10N.getStr("variablesCloseButtonTooltip"),

  /**
   * Specifies the context menu attribute set on variables and properties.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  contextMenuId: "",

  /**
   * The separator label between the variables or properties name and value.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  separatorStr: L10N.getStr("variablesSeparatorLabel"),

  /**
   * Specifies if enumerable properties and variables should be displayed.
   * These variables and properties are visible by default.
   * @param boolean aFlag
   */
  set enumVisible(aFlag) {
    this._enumVisible = aFlag;

    for (let scope of this._store) {
      scope._enumVisible = aFlag;
    }
  },

  /**
   * Specifies if non-enumerable properties and variables should be displayed.
   * These variables and properties are visible by default.
   * @param boolean aFlag
   */
  set nonEnumVisible(aFlag) {
    this._nonEnumVisible = aFlag;

    for (let scope of this._store) {
      scope._nonEnumVisible = aFlag;
    }
  },

  /**
   * Specifies if only enumerable properties and variables should be displayed.
   * Both types of these variables and properties are visible by default.
   * @param boolean aFlag
   */
  set onlyEnumVisible(aFlag) {
    if (aFlag) {
      this.enumVisible = true;
      this.nonEnumVisible = false;
    } else {
      this.enumVisible = true;
      this.nonEnumVisible = true;
    }
  },

  /**
   * Sets if the variable and property searching is enabled.
   * @param boolean aFlag
   */
  set searchEnabled(aFlag) {
    aFlag ? this._enableSearch() : this._disableSearch();
  },

  /**
   * Gets if the variable and property searching is enabled.
   * @return boolean
   */
  get searchEnabled() {
    return !!this._searchboxContainer;
  },

  /**
   * Sets the text displayed for the searchbox in this container.
   * @param string aValue
   */
  set searchPlaceholder(aValue) {
    if (this._searchboxNode) {
      this._searchboxNode.setAttribute("placeholder", aValue);
    }
    this._searchboxPlaceholder = aValue;
  },

  /**
   * Gets the text displayed for the searchbox in this container.
   * @return string
   */
  get searchPlaceholder() {
    return this._searchboxPlaceholder;
  },

  /**
   * Enables variable and property searching in this view.
   * Use the "searchEnabled" setter to enable searching.
   */
  _enableSearch: function() {
    // If searching was already enabled, no need to re-enable it again.
    if (this._searchboxContainer) {
      return;
    }
    let document = this.document;
    let ownerNode = this._parent.parentNode;

    let container = this._searchboxContainer = document.createElement("hbox");
    container.className = "devtools-toolbar";

    // Hide the variables searchbox container if there are no variables or
    // properties to display.
    container.hidden = !this._store.length;

    let searchbox = this._searchboxNode = document.createElement("textbox");
    searchbox.className = "variables-view-searchinput devtools-filterinput";
    searchbox.setAttribute("placeholder", this._searchboxPlaceholder);
    searchbox.setAttribute("type", "search");
    searchbox.setAttribute("flex", "1");
    searchbox.addEventListener("command", this._onSearchboxInput);
    searchbox.addEventListener("keydown", this._onSearchboxKeyDown);

    container.appendChild(searchbox);
    ownerNode.insertBefore(container, this._parent);
  },

  /**
   * Disables variable and property searching in this view.
   * Use the "searchEnabled" setter to disable searching.
   */
  _disableSearch: function() {
    // If searching was already disabled, no need to re-disable it again.
    if (!this._searchboxContainer) {
      return;
    }
    this._searchboxContainer.remove();
    this._searchboxNode.removeEventListener("command", this._onSearchboxInput);
    this._searchboxNode.removeEventListener("keydown", this._onSearchboxKeyDown);

    this._searchboxContainer = null;
    this._searchboxNode = null;
  },

  /**
   * Sets the variables searchbox container hidden or visible.
   * It's hidden by default.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  _toggleSearchVisibility: function(aVisibleFlag) {
    // If searching was already disabled, there's no need to hide it.
    if (!this._searchboxContainer) {
      return;
    }
    this._searchboxContainer.hidden = !aVisibleFlag;
  },

  /**
   * Listener handling the searchbox input event.
   */
  _onSearchboxInput: function() {
    this.scheduleSearch(this._searchboxNode.value);
  },

  /**
   * Listener handling the searchbox keydown event.
   */
  _onSearchboxKeyDown: function(e) {
    switch (e.keyCode) {
      case KeyCodes.DOM_VK_RETURN:
        this._onSearchboxInput();
        return;
      case KeyCodes.DOM_VK_ESCAPE:
        this._searchboxNode.value = "";
        this._onSearchboxInput();
    }
  },

  /**
   * Schedules searching for variables or properties matching the query.
   *
   * @param string aToken
   *        The variable or property to search for.
   * @param number aWait
   *        The amount of milliseconds to wait until draining.
   */
  scheduleSearch: function(aToken, aWait) {
    // Check if this search operation may not be executed lazily.
    if (!this.lazySearch) {
      this._doSearch(aToken);
      return;
    }

    // The amount of time to wait for the requests to settle.
    let maxDelay = SEARCH_ACTION_MAX_DELAY;
    let delay = aWait === undefined ? maxDelay / aToken.length : aWait;

    // Allow requests to settle down first.
    setNamedTimeout("vview-search", delay, () => this._doSearch(aToken));
  },

  /**
   * Performs a case insensitive search for variables or properties matching
   * the query, and hides non-matched items.
   *
   * If aToken is falsy, then all the scopes are unhidden and expanded,
   * while the available variables and properties inside those scopes are
   * just unhidden.
   *
   * @param string aToken
   *        The variable or property to search for.
   */
  _doSearch: function(aToken) {
    if (this.controller && this.controller.supportsSearch()) {
      // Retrieve the main Scope in which we add attributes
      let scope = this._store[0]._store.get(undefined);
      if (!aToken) {
        // Prune the view from old previous content
        // so that we delete the intermediate search results
        // we created in previous searches
        for (let property of scope._store.values()) {
          property.remove();
        }
      }
      // Retrieve new attributes eventually hidden in splits
      this.controller.performSearch(scope, aToken);
      // Filter already displayed attributes
      if (aToken) {
        scope._performSearch(aToken.toLowerCase());
      }
      return;
    }
    for (let scope of this._store) {
      switch (aToken) {
        case "":
        case null:
        case undefined:
          scope.expand();
          scope._performSearch("");
          break;
        default:
          scope._performSearch(aToken.toLowerCase());
          break;
      }
    }
  },

  /**
   * Find the first item in the tree of visible items in this container that
   * matches the predicate. Searches in visual order (the order seen by the
   * user). Descends into each scope to check the scope and its children.
   *
   * @param function aPredicate
   *        A function that returns true when a match is found.
   * @return Scope | Variable | Property
   *         The first visible scope, variable or property, or null if nothing
   *         is found.
   */
  _findInVisibleItems: function(aPredicate) {
    for (let scope of this._store) {
      let result = scope._findInVisibleItems(aPredicate);
      if (result) {
        return result;
      }
    }
    return null;
  },

  /**
   * Find the last item in the tree of visible items in this container that
   * matches the predicate. Searches in reverse visual order (opposite of the
   * order seen by the user). Descends into each scope to check the scope and
   * its children.
   *
   * @param function aPredicate
   *        A function that returns true when a match is found.
   * @return Scope | Variable | Property
   *         The last visible scope, variable or property, or null if nothing
   *         is found.
   */
  _findInVisibleItemsReverse: function(aPredicate) {
    for (let i = this._store.length - 1; i >= 0; i--) {
      let scope = this._store[i];
      let result = scope._findInVisibleItemsReverse(aPredicate);
      if (result) {
        return result;
      }
    }
    return null;
  },

  /**
   * Gets the scope at the specified index.
   *
   * @param number aIndex
   *        The scope's index.
   * @return Scope
   *         The scope if found, undefined if not.
   */
  getScopeAtIndex: function(aIndex) {
    return this._store[aIndex];
  },

  /**
   * Recursively searches this container for the scope, variable or property
   * displayed by the specified node.
   *
   * @param nsIDOMNode aNode
   *        The node to search for.
   * @return Scope | Variable | Property
   *         The matched scope, variable or property, or null if nothing is found.
   */
  getItemForNode: function(aNode) {
    return this._itemsByElement.get(aNode);
  },

  /**
   * Gets the scope owning a Variable or Property.
   *
   * @param Variable | Property
   *        The variable or property to retrieven the owner scope for.
   * @return Scope
   *         The owner scope.
   */
  getOwnerScopeForVariableOrProperty: function(aItem) {
    if (!aItem) {
      return null;
    }
    // If this is a Scope, return it.
    if (!(aItem instanceof Variable)) {
      return aItem;
    }
    // If this is a Variable or Property, find its owner scope.
    if (aItem instanceof Variable && aItem.ownerView) {
      return this.getOwnerScopeForVariableOrProperty(aItem.ownerView);
    }
    return null;
  },

  /**
   * Gets the parent scopes for a specified Variable or Property.
   * The returned list will not include the owner scope.
   *
   * @param Variable | Property
   *        The variable or property for which to find the parent scopes.
   * @return array
   *         A list of parent Scopes.
   */
  getParentScopesForVariableOrProperty: function(aItem) {
    let scope = this.getOwnerScopeForVariableOrProperty(aItem);
    return this._store.slice(0, Math.max(this._store.indexOf(scope), 0));
  },

  /**
   * Gets the currently focused scope, variable or property in this view.
   *
   * @return Scope | Variable | Property
   *         The focused scope, variable or property, or null if nothing is found.
   */
  getFocusedItem: function() {
    let focused = this.document.commandDispatcher.focusedElement;
    return this.getItemForNode(focused);
  },

  /**
   * Focuses the first visible scope, variable, or property in this container.
   */
  focusFirstVisibleItem: function() {
    let focusableItem = this._findInVisibleItems(item => item.focusable);
    if (focusableItem) {
      this._focusItem(focusableItem);
    }
    this._parent.scrollTop = 0;
    this._parent.scrollLeft = 0;
  },

  /**
   * Focuses the last visible scope, variable, or property in this container.
   */
  focusLastVisibleItem: function() {
    let focusableItem = this._findInVisibleItemsReverse(item => item.focusable);
    if (focusableItem) {
      this._focusItem(focusableItem);
    }
    this._parent.scrollTop = this._parent.scrollHeight;
    this._parent.scrollLeft = 0;
  },

  /**
   * Focuses the next scope, variable or property in this view.
   */
  focusNextItem: function() {
    this.focusItemAtDelta(+1);
  },

  /**
   * Focuses the previous scope, variable or property in this view.
   */
  focusPrevItem: function() {
    this.focusItemAtDelta(-1);
  },

  /**
   * Focuses another scope, variable or property in this view, based on
   * the index distance from the currently focused item.
   *
   * @param number aDelta
   *        A scalar specifying by how many items should the selection change.
   */
  focusItemAtDelta: function(aDelta) {
    let direction = aDelta > 0 ? "advanceFocus" : "rewindFocus";
    let distance = Math.abs(Math[aDelta > 0 ? "ceil" : "floor"](aDelta));
    while (distance--) {
      if (!this._focusChange(direction)) {
        break; // Out of bounds.
      }
    }
  },

  /**
   * Focuses the next or previous scope, variable or property in this view.
   *
   * @param string aDirection
   *        Either "advanceFocus" or "rewindFocus".
   * @return boolean
   *         False if the focus went out of bounds and the first or last element
   *         in this view was focused instead.
   */
  _focusChange: function(aDirection) {
    let commandDispatcher = this.document.commandDispatcher;
    let prevFocusedElement = commandDispatcher.focusedElement;
    let currFocusedItem = null;

    do {
      commandDispatcher[aDirection]();

      // Make sure the newly focused item is a part of this view.
      // If the focus goes out of bounds, revert the previously focused item.
      if (!(currFocusedItem = this.getFocusedItem())) {
        prevFocusedElement.focus();
        return false;
      }
    } while (!currFocusedItem.focusable);

    // Focus remained within bounds.
    return true;
  },

  /**
   * Focuses a scope, variable or property and makes sure it's visible.
   *
   * @param aItem Scope | Variable | Property
   *        The item to focus.
   * @param boolean aCollapseFlag
   *        True if the focused item should also be collapsed.
   * @return boolean
   *         True if the item was successfully focused.
   */
  _focusItem: function(aItem, aCollapseFlag) {
    if (!aItem.focusable) {
      return false;
    }
    if (aCollapseFlag) {
      aItem.collapse();
    }
    aItem._target.focus();
    this.boxObject.ensureElementIsVisible(aItem._arrow);
    return true;
  },

  /**
   * Listener handling a key down event on the view.
   */
  _onViewKeyDown: function(e) {
    let item = this.getFocusedItem();

    // Prevent scrolling when pressing navigation keys.
    ViewHelpers.preventScrolling(e);

    switch (e.keyCode) {
      case KeyCodes.DOM_VK_C:
        // Copy current selection to clipboard.
        if (e.ctrlKey || e.metaKey) {
          let item = this.getFocusedItem();
          clipboardHelper.copyString(
            item._nameString + item.separatorStr + item._valueString
          );
        }
        return;

      case KeyCodes.DOM_VK_UP:
        // Always rewind focus.
        this.focusPrevItem(true);
        return;

      case KeyCodes.DOM_VK_DOWN:
        // Always advance focus.
        this.focusNextItem(true);
        return;

      case KeyCodes.DOM_VK_LEFT:
        // Collapse scopes, variables and properties before rewinding focus.
        if (item._isExpanded && item._isArrowVisible) {
          item.collapse();
        } else {
          this._focusItem(item.ownerView);
        }
        return;

      case KeyCodes.DOM_VK_RIGHT:
        // Nothing to do here if this item never expands.
        if (!item._isArrowVisible) {
          return;
        }
        // Expand scopes, variables and properties before advancing focus.
        if (!item._isExpanded) {
          item.expand();
        } else {
          this.focusNextItem(true);
        }
        return;

      case KeyCodes.DOM_VK_PAGE_UP:
        // Rewind a certain number of elements based on the container height.
        this.focusItemAtDelta(-(this.scrollPageSize || Math.min(Math.floor(this._list.scrollHeight /
          PAGE_SIZE_SCROLL_HEIGHT_RATIO),
          PAGE_SIZE_MAX_JUMPS)));
        return;

      case KeyCodes.DOM_VK_PAGE_DOWN:
        // Advance a certain number of elements based on the container height.
        this.focusItemAtDelta(+(this.scrollPageSize || Math.min(Math.floor(this._list.scrollHeight /
          PAGE_SIZE_SCROLL_HEIGHT_RATIO),
          PAGE_SIZE_MAX_JUMPS)));
        return;

      case KeyCodes.DOM_VK_HOME:
        this.focusFirstVisibleItem();
        return;

      case KeyCodes.DOM_VK_END:
        this.focusLastVisibleItem();
        return;

      case KeyCodes.DOM_VK_RETURN:
        // Start editing the value or name of the Variable or Property.
        if (item instanceof Variable) {
          if (e.metaKey || e.altKey || e.shiftKey) {
            item._activateNameInput();
          } else {
            item._activateValueInput();
          }
        }
        return;

      case KeyCodes.DOM_VK_DELETE:
      case KeyCodes.DOM_VK_BACK_SPACE:
        // Delete the Variable or Property if allowed.
        if (item instanceof Variable) {
          item._onDelete(e);
        }
        return;

      case KeyCodes.DOM_VK_INSERT:
        item._onAddProperty(e);
    }
  },

  /**
   * Sets the text displayed in this container when there are no available items.
   * @param string aValue
   */
  set emptyText(aValue) {
    if (this._emptyTextNode) {
      this._emptyTextNode.setAttribute("value", aValue);
    }
    this._emptyTextValue = aValue;
    this._appendEmptyNotice();
  },

  /**
   * Creates and appends a label signaling that this container is empty.
   */
  _appendEmptyNotice: function() {
    if (this._emptyTextNode || !this._emptyTextValue) {
      return;
    }

    let label = this.document.createElement("label");
    label.className = "variables-view-empty-notice";
    label.setAttribute("value", this._emptyTextValue);

    this._parent.appendChild(label);
    this._emptyTextNode = label;
  },

  /**
   * Removes the label signaling that this container is empty.
   */
  _removeEmptyNotice: function() {
    if (!this._emptyTextNode) {
      return;
    }

    this._parent.removeChild(this._emptyTextNode);
    this._emptyTextNode = null;
  },

  /**
   * Gets if all values should be aligned together.
   * @return boolean
   */
  get alignedValues() {
    return this._alignedValues;
  },

  /**
   * Sets if all values should be aligned together.
   * @param boolean aFlag
   */
  set alignedValues(aFlag) {
    this._alignedValues = aFlag;
    if (aFlag) {
      this._parent.setAttribute("aligned-values", "");
    } else {
      this._parent.removeAttribute("aligned-values");
    }
  },

  /**
   * Gets if action buttons (like delete) should be placed at the beginning or
   * end of a line.
   * @return boolean
   */
  get actionsFirst() {
    return this._actionsFirst;
  },

  /**
   * Sets if action buttons (like delete) should be placed at the beginning or
   * end of a line.
   * @param boolean aFlag
   */
  set actionsFirst(aFlag) {
    this._actionsFirst = aFlag;
    if (aFlag) {
      this._parent.setAttribute("actions-first", "");
    } else {
      this._parent.removeAttribute("actions-first");
    }
  },

  /**
   * Gets the parent node holding this view.
   * @return nsIDOMNode
   */
  get boxObject() {
    return this._list.boxObject;
  },

  /**
   * Gets the parent node holding this view.
   * @return nsIDOMNode
   */
  get parentNode() {
    return this._parent;
  },

  /**
   * Gets the owner document holding this view.
   * @return nsIHTMLDocument
   */
  get document() {
    return this._document || (this._document = this._parent.ownerDocument);
  },

  /**
   * Gets the default window holding this view.
   * @return nsIDOMWindow
   */
  get window() {
    return this._window || (this._window = this.document.defaultView);
  },

  _document: null,
  _window: null,

  _store: null,
  _itemsByElement: null,
  _prevHierarchy: null,
  _currHierarchy: null,

  _enumVisible: true,
  _nonEnumVisible: true,
  _alignedValues: false,
  _actionsFirst: false,

  _parent: null,
  _list: null,
  _searchboxNode: null,
  _searchboxContainer: null,
  _searchboxPlaceholder: "",
  _emptyTextNode: null,
  _emptyTextValue: ""
};

VariablesView.NON_SORTABLE_CLASSES = [
  "Array",
  "Int8Array",
  "Uint8Array",
  "Uint8ClampedArray",
  "Int16Array",
  "Uint16Array",
  "Int32Array",
  "Uint32Array",
  "Float32Array",
  "Float64Array",
  "NodeList"
];

/**
 * Determine whether an object's properties should be sorted based on its class.
 *
 * @param string aClassName
 *        The class of the object.
 */
VariablesView.isSortable = function(aClassName) {
  return !VariablesView.NON_SORTABLE_CLASSES.includes(aClassName);
};

/**
 * Generates the string evaluated when performing simple value changes.
 *
 * @param Variable | Property aItem
 *        The current variable or property.
 * @param string aCurrentString
 *        The trimmed user inputted string.
 * @param string aPrefix [optional]
 *        Prefix for the symbolic name.
 * @return string
 *         The string to be evaluated.
 */
VariablesView.simpleValueEvalMacro = function(aItem, aCurrentString, aPrefix = "") {
  return aPrefix + aItem.symbolicName + "=" + aCurrentString;
};

/**
 * Generates the string evaluated when overriding getters and setters with
 * plain values.
 *
 * @param Property aItem
 *        The current getter or setter property.
 * @param string aCurrentString
 *        The trimmed user inputted string.
 * @param string aPrefix [optional]
 *        Prefix for the symbolic name.
 * @return string
 *         The string to be evaluated.
 */
VariablesView.overrideValueEvalMacro = function(aItem, aCurrentString, aPrefix = "") {
  let property = escapeString(aItem._nameString);
  let parent = aPrefix + aItem.ownerView.symbolicName || "this";

  return "Object.defineProperty(" + parent + "," + property + "," +
    "{ value: " + aCurrentString +
    ", enumerable: " + parent + ".propertyIsEnumerable(" + property + ")" +
    ", configurable: true" +
    ", writable: true" +
    "})";
};

/**
 * Generates the string evaluated when performing getters and setters changes.
 *
 * @param Property aItem
 *        The current getter or setter property.
 * @param string aCurrentString
 *        The trimmed user inputted string.
 * @param string aPrefix [optional]
 *        Prefix for the symbolic name.
 * @return string
 *         The string to be evaluated.
 */
VariablesView.getterOrSetterEvalMacro = function(aItem, aCurrentString, aPrefix = "") {
  let type = aItem._nameString;
  let propertyObject = aItem.ownerView;
  let parentObject = propertyObject.ownerView;
  let property = escapeString(propertyObject._nameString);
  let parent = aPrefix + parentObject.symbolicName || "this";

  switch (aCurrentString) {
    case "":
    case "null":
    case "undefined":
      let mirrorType = type == "get" ? "set" : "get";
      let mirrorLookup = type == "get" ? "__lookupSetter__" : "__lookupGetter__";

      // If the parent object will end up without any getter or setter,
      // morph it into a plain value.
      if ((type == "set" && propertyObject.getter.type == "undefined") ||
          (type == "get" && propertyObject.setter.type == "undefined")) {
        // Make sure the right getter/setter to value override macro is applied
        // to the target object.
        return propertyObject.evaluationMacro(propertyObject, "undefined", aPrefix);
      }

      // Construct and return the getter/setter removal evaluation string.
      // e.g: Object.defineProperty(foo, "bar", {
      //   get: foo.__lookupGetter__("bar"),
      //   set: undefined,
      //   enumerable: true,
      //   configurable: true
      // })
      return "Object.defineProperty(" + parent + "," + property + "," +
        "{" + mirrorType + ":" + parent + "." + mirrorLookup + "(" + property + ")" +
        "," + type + ":" + undefined +
        ", enumerable: " + parent + ".propertyIsEnumerable(" + property + ")" +
        ", configurable: true" +
        "})";

    default:
      // Wrap statements inside a function declaration if not already wrapped.
      if (!aCurrentString.startsWith("function")) {
        let header = "function(" + (type == "set" ? "value" : "") + ")";
        let body = "";
        // If there's a return statement explicitly written, always use the
        // standard function definition syntax
        if (aCurrentString.includes("return ")) {
          body = "{" + aCurrentString + "}";
        } else if (aCurrentString.startsWith("{")) {
          // If block syntax is used, use the whole string as the function body.
          body = aCurrentString;
        } else {
          // Prefer an expression closure.
          body = "(" + aCurrentString + ")";
        }
        aCurrentString = header + body;
      }

      // Determine if a new getter or setter should be defined.
      let defineType = type == "get" ? "__defineGetter__" : "__defineSetter__";

      // Make sure all quotes are escaped in the expression's syntax,
      let defineFunc = "eval(\"(" + aCurrentString.replace(/"/g, "\\$&") + ")\")";

      // Construct and return the getter/setter evaluation string.
      // e.g: foo.__defineGetter__("bar", eval("(function() { return 42; })"))
      return parent + "." + defineType + "(" + property + "," + defineFunc + ")";
  }
};

/**
 * Function invoked when a getter or setter is deleted.
 *
 * @param Property aItem
 *        The current getter or setter property.
 */
VariablesView.getterOrSetterDeleteCallback = function(aItem) {
  aItem._disable();

  // Make sure the right getter/setter to value override macro is applied
  // to the target object.
  aItem.ownerView.eval(aItem, "");

  return true; // Don't hide the element.
};

/**
 * A Scope is an object holding Variable instances.
 * Iterable via "for (let [name, variable] of instance) { }".
 *
 * @param VariablesView aView
 *        The view to contain this scope.
 * @param string aName
 *        The scope's name.
 * @param object aFlags [optional]
 *        Additional options or flags for this scope.
 */
function Scope(aView, aName, aFlags = {}) {
  this.ownerView = aView;

  this._onClick = this._onClick.bind(this);
  this._openEnum = this._openEnum.bind(this);
  this._openNonEnum = this._openNonEnum.bind(this);

  // Inherit properties and flags from the parent view. You can override
  // each of these directly onto any scope, variable or property instance.
  this.scrollPageSize = aView.scrollPageSize;
  this.eval = aView.eval;
  this.switch = aView.switch;
  this.delete = aView.delete;
  this.new = aView.new;
  this.preventDisableOnChange = aView.preventDisableOnChange;
  this.preventDescriptorModifiers = aView.preventDescriptorModifiers;
  this.editableNameTooltip = aView.editableNameTooltip;
  this.editableValueTooltip = aView.editableValueTooltip;
  this.editButtonTooltip = aView.editButtonTooltip;
  this.deleteButtonTooltip = aView.deleteButtonTooltip;
  this.domNodeValueTooltip = aView.domNodeValueTooltip;
  this.contextMenuId = aView.contextMenuId;
  this.separatorStr = aView.separatorStr;

  this._init(aName, aFlags);
}

Scope.prototype = {
  /**
   * Whether this Scope should be prefetched when it is remoted.
   */
  shouldPrefetch: true,

  /**
   * Whether this Scope should paginate its contents.
   */
  allowPaginate: false,

  /**
   * The class name applied to this scope's target element.
   */
  targetClassName: "variables-view-scope",

  /**
   * Create a new Variable that is a child of this Scope.
   *
   * @param string aName
   *        The name of the new Property.
   * @param object aDescriptor
   *        The variable's descriptor.
   * @param object aOptions
   *        Options of the form accepted by addItem.
   * @return Variable
   *         The newly created child Variable.
   */
  _createChild: function(aName, aDescriptor, aOptions) {
    return new Variable(this, aName, aDescriptor, aOptions);
  },

  /**
   * Adds a child to contain any inspected properties.
   *
   * @param string aName
   *        The child's name.
   * @param object aDescriptor
   *        Specifies the value and/or type & class of the child,
   *        or 'get' & 'set' accessor properties. If the type is implicit,
   *        it will be inferred from the value. If this parameter is omitted,
   *        a property without a value will be added (useful for branch nodes).
   *        e.g. - { value: 42 }
   *             - { value: true }
   *             - { value: "nasu" }
   *             - { value: { type: "undefined" } }
   *             - { value: { type: "null" } }
   *             - { value: { type: "object", class: "Object" } }
   *             - { get: { type: "object", class: "Function" },
   *                 set: { type: "undefined" } }
   * @param object aOptions
   *        Specifies some options affecting the new variable.
   *        Recognized properties are
   *        * boolean relaxed  true if name duplicates should be allowed.
   *                           You probably shouldn't do it. Use this
   *                           with caution.
   *        * boolean internalItem  true if the item is internally generated.
   *                           This is used for special variables
   *                           like <return> or <exception> and distinguishes
   *                           them from ordinary properties that happen
   *                           to have the same name
   * @return Variable
   *         The newly created Variable instance, null if it already exists.
   */
  addItem: function(aName, aDescriptor = {}, aOptions = {}) {
    let {relaxed} = aOptions;
    if (this._store.has(aName) && !relaxed) {
      return this._store.get(aName);
    }

    let child = this._createChild(aName, aDescriptor, aOptions);
    this._store.set(aName, child);
    this._variablesView._itemsByElement.set(child._target, child);
    this._variablesView._currHierarchy.set(child.absoluteName, child);
    child.header = aName !== undefined;

    return child;
  },

  /**
   * Adds items for this variable.
   *
   * @param object aItems
   *        An object containing some { name: descriptor } data properties,
   *        specifying the value and/or type & class of the variable,
   *        or 'get' & 'set' accessor properties. If the type is implicit,
   *        it will be inferred from the value.
   *        e.g. - { someProp0: { value: 42 },
   *                 someProp1: { value: true },
   *                 someProp2: { value: "nasu" },
   *                 someProp3: { value: { type: "undefined" } },
   *                 someProp4: { value: { type: "null" } },
   *                 someProp5: { value: { type: "object", class: "Object" } },
   *                 someProp6: { get: { type: "object", class: "Function" },
   *                              set: { type: "undefined" } } }
   * @param object aOptions [optional]
   *        Additional options for adding the properties. Supported options:
   *        - sorted: true to sort all the properties before adding them
   *        - callback: function invoked after each item is added
   */
  addItems: function(aItems, aOptions = {}) {
    let names = Object.keys(aItems);

    // Sort all of the properties before adding them, if preferred.
    if (aOptions.sorted) {
      names.sort(this._naturalSort);
    }

    // Add the properties to the current scope.
    for (let name of names) {
      let descriptor = aItems[name];
      let item = this.addItem(name, descriptor);

      if (aOptions.callback) {
        aOptions.callback(item, descriptor && descriptor.value);
      }
    }
  },

  /**
   * Remove this Scope from its parent and remove all children recursively.
   */
  remove: function() {
    let view = this._variablesView;
    view._store.splice(view._store.indexOf(this), 1);
    view._itemsByElement.delete(this._target);
    view._currHierarchy.delete(this._nameString);

    this._target.remove();

    for (let variable of this._store.values()) {
      variable.remove();
    }
  },

  /**
   * Gets the variable in this container having the specified name.
   *
   * @param string aName
   *        The name of the variable to get.
   * @return Variable
   *         The matched variable, or null if nothing is found.
   */
  get: function(aName) {
    return this._store.get(aName);
  },

  /**
   * Recursively searches for the variable or property in this container
   * displayed by the specified node.
   *
   * @param nsIDOMNode aNode
   *        The node to search for.
   * @return Variable | Property
   *         The matched variable or property, or null if nothing is found.
   */
  find: function(aNode) {
    for (let [, variable] of this._store) {
      let match;
      if (variable._target == aNode) {
        match = variable;
      } else {
        match = variable.find(aNode);
      }
      if (match) {
        return match;
      }
    }
    return null;
  },

  /**
   * Determines if this scope is a direct child of a parent variables view,
   * scope, variable or property.
   *
   * @param VariablesView | Scope | Variable | Property
   *        The parent to check.
   * @return boolean
   *         True if the specified item is a direct child, false otherwise.
   */
  isChildOf: function(aParent) {
    return this.ownerView == aParent;
  },

  /**
   * Determines if this scope is a descendant of a parent variables view,
   * scope, variable or property.
   *
   * @param VariablesView | Scope | Variable | Property
   *        The parent to check.
   * @return boolean
   *         True if the specified item is a descendant, false otherwise.
   */
  isDescendantOf: function(aParent) {
    if (this.isChildOf(aParent)) {
      return true;
    }

    // Recurse to parent if it is a Scope, Variable, or Property.
    if (this.ownerView instanceof Scope) {
      return this.ownerView.isDescendantOf(aParent);
    }

    return false;
  },

  /**
   * Shows the scope.
   */
  show: function() {
    this._target.hidden = false;
    this._isContentVisible = true;

    if (this.onshow) {
      this.onshow(this);
    }
  },

  /**
   * Hides the scope.
   */
  hide: function() {
    this._target.hidden = true;
    this._isContentVisible = false;

    if (this.onhide) {
      this.onhide(this);
    }
  },

  /**
   * Expands the scope, showing all the added details.
   */
  expand: function() {
    if (this._isExpanded || this._isLocked) {
      return;
    }
    if (this._variablesView._enumVisible) {
      this._openEnum();
    }
    if (this._variablesView._nonEnumVisible) {
      Services.tm.dispatchToMainThread({ run: this._openNonEnum });
    }
    this._isExpanded = true;

    if (this.onexpand) {
      // We return onexpand as it sometimes returns a promise
      // (up to the user of VariableView to do it)
      // that can indicate when the view is done expanding
      // and attributes are available. (Mostly used for tests)
      return this.onexpand(this);
    }
  },

  /**
   * Collapses the scope, hiding all the added details.
   */
  collapse: function() {
    if (!this._isExpanded || this._isLocked) {
      return;
    }
    this._arrow.removeAttribute("open");
    this._enum.removeAttribute("open");
    this._nonenum.removeAttribute("open");
    this._isExpanded = false;

    if (this.oncollapse) {
      this.oncollapse(this);
    }
  },

  /**
   * Toggles between the scope's collapsed and expanded state.
   */
  toggle: function(e) {
    if (e && e.button != 0) {
      // Only allow left-click to trigger this event.
      return;
    }
    this.expanded ^= 1;

    // Make sure the scope and its contents are visibile.
    for (let [, variable] of this._store) {
      variable.header = true;
      variable._matched = true;
    }
    if (this.ontoggle) {
      this.ontoggle(this);
    }
  },

  /**
   * Shows the scope's title header.
   */
  showHeader: function() {
    if (this._isHeaderVisible || !this._nameString) {
      return;
    }
    this._target.removeAttribute("untitled");
    this._isHeaderVisible = true;
  },

  /**
   * Hides the scope's title header.
   * This action will automatically expand the scope.
   */
  hideHeader: function() {
    if (!this._isHeaderVisible) {
      return;
    }
    this.expand();
    this._target.setAttribute("untitled", "");
    this._isHeaderVisible = false;
  },

  /**
   * Sort in ascending order
   * This only needs to compare non-numbers since it is dealing with an array
   * which numeric-based indices are placed in order.
   *
   * @param string a
   * @param string b
   * @return number
   *         -1 if a is less than b, 0 if no change in order, +1 if a is greater than 0
   */
  _naturalSort: function(a, b) {
    if (isNaN(parseFloat(a)) && isNaN(parseFloat(b))) {
      return a < b ? -1 : 1;
    }
  },

  /**
   * Shows the scope's expand/collapse arrow.
   */
  showArrow: function() {
    if (this._isArrowVisible) {
      return;
    }
    this._arrow.removeAttribute("invisible");
    this._isArrowVisible = true;
  },

  /**
   * Hides the scope's expand/collapse arrow.
   */
  hideArrow: function() {
    if (!this._isArrowVisible) {
      return;
    }
    this._arrow.setAttribute("invisible", "");
    this._isArrowVisible = false;
  },

  /**
   * Gets the visibility state.
   * @return boolean
   */
  get visible() {
    return this._isContentVisible;
  },

  /**
   * Gets the expanded state.
   * @return boolean
   */
  get expanded() {
    return this._isExpanded;
  },

  /**
   * Gets the header visibility state.
   * @return boolean
   */
  get header() {
    return this._isHeaderVisible;
  },

  /**
   * Gets the twisty visibility state.
   * @return boolean
   */
  get twisty() {
    return this._isArrowVisible;
  },

  /**
   * Gets the expand lock state.
   * @return boolean
   */
  get locked() {
    return this._isLocked;
  },

  /**
   * Sets the visibility state.
   * @param boolean aFlag
   */
  set visible(aFlag) {
    aFlag ? this.show() : this.hide();
  },

  /**
   * Sets the expanded state.
   * @param boolean aFlag
   */
  set expanded(aFlag) {
    aFlag ? this.expand() : this.collapse();
  },

  /**
   * Sets the header visibility state.
   * @param boolean aFlag
   */
  set header(aFlag) {
    aFlag ? this.showHeader() : this.hideHeader();
  },

  /**
   * Sets the twisty visibility state.
   * @param boolean aFlag
   */
  set twisty(aFlag) {
    aFlag ? this.showArrow() : this.hideArrow();
  },

  /**
   * Sets the expand lock state.
   * @param boolean aFlag
   */
  set locked(aFlag) {
    this._isLocked = aFlag;
  },

  /**
   * Specifies if this target node may be focused.
   * @return boolean
   */
  get focusable() {
    // Check if this target node is actually visibile.
    if (!this._nameString ||
        !this._isContentVisible ||
        !this._isHeaderVisible ||
        !this._isMatch) {
      return false;
    }
    // Check if all parent objects are expanded.
    let item = this;

    // Recurse while parent is a Scope, Variable, or Property
    while ((item = item.ownerView) && item instanceof Scope) {
      if (!item._isExpanded) {
        return false;
      }
    }
    return true;
  },

  /**
   * Focus this scope.
   */
  focus: function() {
    this._variablesView._focusItem(this);
  },

  /**
   * Adds an event listener for a certain event on this scope's title.
   * @param string aName
   * @param function aCallback
   * @param boolean aCapture
   */
  addEventListener: function(aName, aCallback, aCapture) {
    this._title.addEventListener(aName, aCallback, aCapture);
  },

  /**
   * Removes an event listener for a certain event on this scope's title.
   * @param string aName
   * @param function aCallback
   * @param boolean aCapture
   */
  removeEventListener: function(aName, aCallback, aCapture) {
    this._title.removeEventListener(aName, aCallback, aCapture);
  },

  /**
   * Gets the id associated with this item.
   * @return string
   */
  get id() {
    return this._idString;
  },

  /**
   * Gets the name associated with this item.
   * @return string
   */
  get name() {
    return this._nameString;
  },

  /**
   * Gets the displayed value for this item.
   * @return string
   */
  get displayValue() {
    return this._valueString;
  },

  /**
   * Gets the class names used for the displayed value.
   * @return string
   */
  get displayValueClassName() {
    return this._valueClassName;
  },

  /**
   * Gets the element associated with this item.
   * @return nsIDOMNode
   */
  get target() {
    return this._target;
  },

  /**
   * Initializes this scope's id, view and binds event listeners.
   *
   * @param string aName
   *        The scope's name.
   * @param object aFlags [optional]
   *        Additional options or flags for this scope.
   */
  _init: function(aName, aFlags) {
    this._idString = generateId(this._nameString = aName);
    this._displayScope(aName, `${this.targetClassName} ${aFlags.customClass}`,
                       "devtools-toolbar");
    this._addEventListeners();
    this.parentNode.appendChild(this._target);
  },

  /**
   * Creates the necessary nodes for this scope.
   *
   * @param string aName
   *        The scope's name.
   * @param string aTargetClassName
   *        A custom class name for this scope's target element.
   * @param string aTitleClassName [optional]
   *        A custom class name for this scope's title element.
   */
  _displayScope: function(aName = "", aTargetClassName, aTitleClassName = "") {
    let document = this.document;

    let element = this._target = document.createElement("vbox");
    element.id = this._idString;
    element.className = aTargetClassName;

    let arrow = this._arrow = document.createElement("hbox");
    arrow.className = "arrow theme-twisty";

    let name = this._name = document.createElement("label");
    name.className = "plain name";
    name.setAttribute("value", aName.trim());
    name.setAttribute("crop", "end");

    let title = this._title = document.createElement("hbox");
    title.className = "title " + aTitleClassName;
    title.setAttribute("align", "center");

    let enumerable = this._enum = document.createElement("vbox");
    let nonenum = this._nonenum = document.createElement("vbox");
    enumerable.className = "variables-view-element-details enum";
    nonenum.className = "variables-view-element-details nonenum";

    title.appendChild(arrow);
    title.appendChild(name);

    element.appendChild(title);
    element.appendChild(enumerable);
    element.appendChild(nonenum);
  },

  /**
   * Adds the necessary event listeners for this scope.
   */
  _addEventListeners: function() {
    this._title.addEventListener("mousedown", this._onClick);
  },

  /**
   * The click listener for this scope's title.
   */
  _onClick: function(e) {
    if (this.editing ||
        e.button != 0 ||
        e.target == this._editNode ||
        e.target == this._deleteNode ||
        e.target == this._addPropertyNode) {
      return;
    }
    this.toggle();
    this.focus();
  },

  /**
   * Opens the enumerable items container.
   */
  _openEnum: function() {
    this._arrow.setAttribute("open", "");
    this._enum.setAttribute("open", "");
  },

  /**
   * Opens the non-enumerable items container.
   */
  _openNonEnum: function() {
    this._nonenum.setAttribute("open", "");
  },

  /**
   * Specifies if enumerable properties and variables should be displayed.
   * @param boolean aFlag
   */
  set _enumVisible(aFlag) {
    for (let [, variable] of this._store) {
      variable._enumVisible = aFlag;

      if (!this._isExpanded) {
        continue;
      }
      if (aFlag) {
        this._enum.setAttribute("open", "");
      } else {
        this._enum.removeAttribute("open");
      }
    }
  },

  /**
   * Specifies if non-enumerable properties and variables should be displayed.
   * @param boolean aFlag
   */
  set _nonEnumVisible(aFlag) {
    for (let [, variable] of this._store) {
      variable._nonEnumVisible = aFlag;

      if (!this._isExpanded) {
        continue;
      }
      if (aFlag) {
        this._nonenum.setAttribute("open", "");
      } else {
        this._nonenum.removeAttribute("open");
      }
    }
  },

  /**
   * Performs a case insensitive search for variables or properties matching
   * the query, and hides non-matched items.
   *
   * @param string aLowerCaseQuery
   *        The lowercased name of the variable or property to search for.
   */
  _performSearch: function(aLowerCaseQuery) {
    for (let [, variable] of this._store) {
      let currentObject = variable;
      let lowerCaseName = variable._nameString.toLowerCase();
      let lowerCaseValue = variable._valueString.toLowerCase();

      // Non-matched variables or properties require a corresponding attribute.
      if (!lowerCaseName.includes(aLowerCaseQuery) &&
          !lowerCaseValue.includes(aLowerCaseQuery)) {
        variable._matched = false;
      } else {
        // Variable or property is matched.
        variable._matched = true;

        // If the variable was ever expanded, there's a possibility it may
        // contain some matched properties, so make sure they're visible
        // ("expand downwards").
        if (variable._store.size) {
          variable.expand();
        }

        // If the variable is contained in another Scope, Variable, or Property,
        // the parent may not be a match, thus hidden. It should be visible
        // ("expand upwards").
        while ((variable = variable.ownerView) && variable instanceof Scope) {
          variable._matched = true;
          variable.expand();
        }
      }

      // Proceed with the search recursively inside this variable or property.
      if (currentObject._store.size || currentObject.getter || currentObject.setter) {
        currentObject._performSearch(aLowerCaseQuery);
      }
    }
  },

  /**
   * Sets if this object instance is a matched or non-matched item.
   * @param boolean aStatus
   */
  set _matched(aStatus) {
    if (this._isMatch == aStatus) {
      return;
    }
    if (aStatus) {
      this._isMatch = true;
      this.target.removeAttribute("unmatched");
    } else {
      this._isMatch = false;
      this.target.setAttribute("unmatched", "");
    }
  },

  /**
   * Find the first item in the tree of visible items in this item that matches
   * the predicate. Searches in visual order (the order seen by the user).
   * Tests itself, then descends into first the enumerable children and then
   * the non-enumerable children (since they are presented in separate groups).
   *
   * @param function aPredicate
   *        A function that returns true when a match is found.
   * @return Scope | Variable | Property
   *         The first visible scope, variable or property, or null if nothing
   *         is found.
   */
  _findInVisibleItems: function(aPredicate) {
    if (aPredicate(this)) {
      return this;
    }

    if (this._isExpanded) {
      if (this._variablesView._enumVisible) {
        for (let item of this._enumItems) {
          let result = item._findInVisibleItems(aPredicate);
          if (result) {
            return result;
          }
        }
      }

      if (this._variablesView._nonEnumVisible) {
        for (let item of this._nonEnumItems) {
          let result = item._findInVisibleItems(aPredicate);
          if (result) {
            return result;
          }
        }
      }
    }

    return null;
  },

  /**
   * Find the last item in the tree of visible items in this item that matches
   * the predicate. Searches in reverse visual order (opposite of the order
   * seen by the user). Descends into first the non-enumerable children, then
   * the enumerable children (since they are presented in separate groups), and
   * finally tests itself.
   *
   * @param function aPredicate
   *        A function that returns true when a match is found.
   * @return Scope | Variable | Property
   *         The last visible scope, variable or property, or null if nothing
   *         is found.
   */
  _findInVisibleItemsReverse: function(aPredicate) {
    if (this._isExpanded) {
      if (this._variablesView._nonEnumVisible) {
        for (let i = this._nonEnumItems.length - 1; i >= 0; i--) {
          let item = this._nonEnumItems[i];
          let result = item._findInVisibleItemsReverse(aPredicate);
          if (result) {
            return result;
          }
        }
      }

      if (this._variablesView._enumVisible) {
        for (let i = this._enumItems.length - 1; i >= 0; i--) {
          let item = this._enumItems[i];
          let result = item._findInVisibleItemsReverse(aPredicate);
          if (result) {
            return result;
          }
        }
      }
    }

    if (aPredicate(this)) {
      return this;
    }

    return null;
  },

  /**
   * Gets top level variables view instance.
   * @return VariablesView
   */
  get _variablesView() {
    return this._topView || (this._topView = (() => {
      let parentView = this.ownerView;
      let topView;

      while ((topView = parentView.ownerView)) {
        parentView = topView;
      }
      return parentView;
    })());
  },

  /**
   * Gets the parent node holding this scope.
   * @return nsIDOMNode
   */
  get parentNode() {
    return this.ownerView._list;
  },

  /**
   * Gets the owner document holding this scope.
   * @return nsIHTMLDocument
   */
  get document() {
    return this._document || (this._document = this.ownerView.document);
  },

  /**
   * Gets the default window holding this scope.
   * @return nsIDOMWindow
   */
  get window() {
    return this._window || (this._window = this.ownerView.window);
  },

  _topView: null,
  _document: null,
  _window: null,

  ownerView: null,
  eval: null,
  switch: null,
  delete: null,
  new: null,
  preventDisableOnChange: false,
  preventDescriptorModifiers: false,
  editing: false,
  editableNameTooltip: "",
  editableValueTooltip: "",
  editButtonTooltip: "",
  deleteButtonTooltip: "",
  domNodeValueTooltip: "",
  contextMenuId: "",
  separatorStr: "",

  _store: null,
  _enumItems: null,
  _nonEnumItems: null,
  _fetched: false,
  _committed: false,
  _isLocked: false,
  _isExpanded: false,
  _isContentVisible: true,
  _isHeaderVisible: true,
  _isArrowVisible: true,
  _isMatch: true,
  _idString: "",
  _nameString: "",
  _target: null,
  _arrow: null,
  _name: null,
  _title: null,
  _enum: null,
  _nonenum: null,
};

// Creating maps and arrays thousands of times for variables or properties
// with a large number of children fills up a lot of memory. Make sure
// these are instantiated only if needed.
DevToolsUtils.defineLazyPrototypeGetter(Scope.prototype, "_store", () => new Map());
DevToolsUtils.defineLazyPrototypeGetter(Scope.prototype, "_enumItems", Array);
DevToolsUtils.defineLazyPrototypeGetter(Scope.prototype, "_nonEnumItems", Array);

/**
 * A Variable is a Scope holding Property instances.
 * Iterable via "for (let [name, property] of instance) { }".
 *
 * @param Scope aScope
 *        The scope to contain this variable.
 * @param string aName
 *        The variable's name.
 * @param object aDescriptor
 *        The variable's descriptor.
 * @param object aOptions
 *        Options of the form accepted by Scope.addItem
 */
function Variable(aScope, aName, aDescriptor, aOptions) {
  this._setTooltips = this._setTooltips.bind(this);
  this._activateNameInput = this._activateNameInput.bind(this);
  this._activateValueInput = this._activateValueInput.bind(this);
  this.openNodeInInspector = this.openNodeInInspector.bind(this);
  this.highlightDomNode = this.highlightDomNode.bind(this);
  this.unhighlightDomNode = this.unhighlightDomNode.bind(this);
  this._internalItem = aOptions.internalItem;

  // Treat safe getter descriptors as descriptors with a value.
  if ("getterValue" in aDescriptor) {
    aDescriptor.value = aDescriptor.getterValue;
    delete aDescriptor.get;
    delete aDescriptor.set;
  }

  Scope.call(this, aScope, aName, this._initialDescriptor = aDescriptor);
  this.setGrip(aDescriptor.value);
}

Variable.prototype = extend(Scope.prototype, {
  /**
   * Whether this Variable should be prefetched when it is remoted.
   */
  get shouldPrefetch() {
    return this.name == "window" || this.name == "this";
  },

  /**
   * Whether this Variable should paginate its contents.
   */
  get allowPaginate() {
    return this.name != "window" && this.name != "this";
  },

  /**
   * The class name applied to this variable's target element.
   */
  targetClassName: "variables-view-variable variable-or-property",

  /**
   * Create a new Property that is a child of Variable.
   *
   * @param string aName
   *        The name of the new Property.
   * @param object aDescriptor
   *        The property's descriptor.
   * @param object aOptions
   *        Options of the form accepted by Scope.addItem
   * @return Property
   *         The newly created child Property.
   */
  _createChild: function(aName, aDescriptor, aOptions) {
    return new Property(this, aName, aDescriptor, aOptions);
  },

  /**
   * Remove this Variable from its parent and remove all children recursively.
   */
  remove: function() {
    if (this._linkedToInspector) {
      this.unhighlightDomNode();
      this._valueLabel.removeEventListener("mouseover", this.highlightDomNode);
      this._valueLabel.removeEventListener("mouseout", this.unhighlightDomNode);
      this._openInspectorNode.removeEventListener("mousedown", this.openNodeInInspector);
    }

    this.ownerView._store.delete(this._nameString);
    this._variablesView._itemsByElement.delete(this._target);
    this._variablesView._currHierarchy.delete(this.absoluteName);

    this._target.remove();

    for (let property of this._store.values()) {
      property.remove();
    }
  },

  /**
   * Populates this variable to contain all the properties of an object.
   *
   * @param object aObject
   *        The raw object you want to display.
   * @param object aOptions [optional]
   *        Additional options for adding the properties. Supported options:
   *        - sorted: true to sort all the properties before adding them
   *        - expanded: true to expand all the properties after adding them
   */
  populate: function(aObject, aOptions = {}) {
    // Retrieve the properties only once.
    if (this._fetched) {
      return;
    }
    this._fetched = true;

    let propertyNames = Object.getOwnPropertyNames(aObject);
    let prototype = Object.getPrototypeOf(aObject);

    // Sort all of the properties before adding them, if preferred.
    if (aOptions.sorted) {
      propertyNames.sort(this._naturalSort);
    }

    // Add all the variable properties.
    for (let name of propertyNames) {
      let descriptor = Object.getOwnPropertyDescriptor(aObject, name);
      if (descriptor.get || descriptor.set) {
        let prop = this._addRawNonValueProperty(name, descriptor);
        if (aOptions.expanded) {
          prop.expanded = true;
        }
      } else {
        let prop = this._addRawValueProperty(name, descriptor, aObject[name]);
        if (aOptions.expanded) {
          prop.expanded = true;
        }
      }
    }
    // Add the variable's __proto__.
    if (prototype) {
      this._addRawValueProperty("__proto__", {}, prototype);
    }
  },

  /**
   * Populates a specific variable or property instance to contain all the
   * properties of an object
   *
   * @param Variable | Property aVar
   *        The target variable to populate.
   * @param object aObject [optional]
   *        The raw object you want to display. If unspecified, the object is
   *        assumed to be defined in a _sourceValue property on the target.
   */
  _populateTarget: function(aVar, aObject = aVar._sourceValue) {
    aVar.populate(aObject);
  },

  /**
   * Adds a property for this variable based on a raw value descriptor.
   *
   * @param string aName
   *        The property's name.
   * @param object aDescriptor
   *        Specifies the exact property descriptor as returned by a call to
   *        Object.getOwnPropertyDescriptor.
   * @param object aValue
   *        The raw property value you want to display.
   * @return Property
   *         The newly added property instance.
   */
  _addRawValueProperty: function(aName, aDescriptor, aValue) {
    let descriptor = Object.create(aDescriptor);
    descriptor.value = VariablesView.getGrip(aValue);

    let propertyItem = this.addItem(aName, descriptor);
    propertyItem._sourceValue = aValue;

    // Add an 'onexpand' callback for the property, lazily handling
    // the addition of new child properties.
    if (!VariablesView.isPrimitive(descriptor)) {
      propertyItem.onexpand = this._populateTarget;
    }
    return propertyItem;
  },

  /**
   * Adds a property for this variable based on a getter/setter descriptor.
   *
   * @param string aName
   *        The property's name.
   * @param object aDescriptor
   *        Specifies the exact property descriptor as returned by a call to
   *        Object.getOwnPropertyDescriptor.
   * @return Property
   *         The newly added property instance.
   */
  _addRawNonValueProperty: function(aName, aDescriptor) {
    let descriptor = Object.create(aDescriptor);
    descriptor.get = VariablesView.getGrip(aDescriptor.get);
    descriptor.set = VariablesView.getGrip(aDescriptor.set);

    return this.addItem(aName, descriptor);
  },

  /**
   * Gets this variable's path to the topmost scope in the form of a string
   * meant for use via eval() or a similar approach.
   * For example, a symbolic name may look like "arguments['0']['foo']['bar']".
   * @return string
   */
  get symbolicName() {
    return this._nameString || "";
  },

  /**
   * Gets full path to this variable, including name of the scope.
   * @return string
   */
  get absoluteName() {
    if (this._absoluteName) {
      return this._absoluteName;
    }

    this._absoluteName = this.ownerView._nameString + "[" + escapeString(this._nameString) + "]";
    return this._absoluteName;
  },

  /**
   * Gets this variable's symbolic path to the topmost scope.
   * @return array
   * @see Variable._buildSymbolicPath
   */
  get symbolicPath() {
    if (this._symbolicPath) {
      return this._symbolicPath;
    }
    this._symbolicPath = this._buildSymbolicPath();
    return this._symbolicPath;
  },

  /**
   * Build this variable's path to the topmost scope in form of an array of
   * strings, one for each segment of the path.
   * For example, a symbolic path may look like ["0", "foo", "bar"].
   * @return array
   */
  _buildSymbolicPath: function(path = []) {
    if (this.name) {
      path.unshift(this.name);
      if (this.ownerView instanceof Variable) {
        return this.ownerView._buildSymbolicPath(path);
      }
    }
    return path;
  },

  /**
   * Returns this variable's value from the descriptor if available.
   * @return any
   */
  get value() {
    return this._initialDescriptor.value;
  },

  /**
   * Returns this variable's getter from the descriptor if available.
   * @return object
   */
  get getter() {
    return this._initialDescriptor.get;
  },

  /**
   * Returns this variable's getter from the descriptor if available.
   * @return object
   */
  get setter() {
    return this._initialDescriptor.set;
  },

  /**
   * Sets the specific grip for this variable (applies the text content and
   * class name to the value label).
   *
   * The grip should contain the value or the type & class, as defined in the
   * remote debugger protocol. For convenience, undefined and null are
   * both considered types.
   *
   * @param any aGrip
   *        Specifies the value and/or type & class of the variable.
   *        e.g. - 42
   *             - true
   *             - "nasu"
   *             - { type: "undefined" }
   *             - { type: "null" }
   *             - { type: "object", class: "Object" }
   */
  setGrip: function(aGrip) {
    // Don't allow displaying grip information if there's no name available
    // or the grip is malformed.
    if (this._nameString === undefined || aGrip === undefined || aGrip === null) {
      return;
    }
    // Getters and setters should display grip information in sub-properties.
    if (this.getter || this.setter) {
      return;
    }

    let prevGrip = this._valueGrip;
    if (prevGrip) {
      this._valueLabel.classList.remove(VariablesView.getClass(prevGrip));
    }
    this._valueGrip = aGrip;

    if (aGrip && (aGrip.optimizedOut || aGrip.uninitialized || aGrip.missingArguments)) {
      if (aGrip.optimizedOut) {
        this._valueString = L10N.getStr("variablesViewOptimizedOut");
      } else if (aGrip.uninitialized) {
        this._valueString = L10N.getStr("variablesViewUninitialized");
      } else if (aGrip.missingArguments) {
        this._valueString = L10N.getStr("variablesViewMissingArgs");
      }
      this.eval = null;
    } else {
      this._valueString = VariablesView.getString(aGrip, {
        concise: true,
        noEllipsis: true,
      });
      this.eval = this.ownerView.eval;
    }

    this._valueClassName = VariablesView.getClass(aGrip);

    this._valueLabel.classList.add(this._valueClassName);
    this._valueLabel.setAttribute("value", this._valueString);
    this._separatorLabel.hidden = false;

    // DOMNodes get special treatment since they can be linked to the inspector
    if (this._valueGrip.preview && this._valueGrip.preview.kind === "DOMNode") {
      this._linkToInspector();
    }
  },

  /**
   * Marks this variable as overridden.
   *
   * @param boolean aFlag
   *        Whether this variable is overridden or not.
   */
  setOverridden: function(aFlag) {
    if (aFlag) {
      this._target.setAttribute("overridden", "");
    } else {
      this._target.removeAttribute("overridden");
    }
  },

  /**
   * Briefly flashes this variable.
   *
   * @param number aDuration [optional]
   *        An optional flash animation duration.
   */
  flash: function(aDuration = ITEM_FLASH_DURATION) {
    let fadeInDelay = this._variablesView.lazyEmptyDelay + 1;
    let fadeOutDelay = fadeInDelay + aDuration;

    setNamedTimeout("vview-flash-in" + this.absoluteName,
      fadeInDelay, () => this._target.setAttribute("changed", ""));

    setNamedTimeout("vview-flash-out" + this.absoluteName,
      fadeOutDelay, () => this._target.removeAttribute("changed"));
  },

  /**
   * Initializes this variable's id, view and binds event listeners.
   *
   * @param string aName
   *        The variable's name.
   * @param object aDescriptor
   *        The variable's descriptor.
   */
  _init: function(aName, aDescriptor) {
    this._idString = generateId(this._nameString = aName);
    this._displayScope(aName, this.targetClassName);
    this._displayVariable();
    this._customizeVariable();
    this._prepareTooltips();
    this._setAttributes();
    this._addEventListeners();

    if (this._initialDescriptor.enumerable ||
        this._nameString == "this" ||
        this._internalItem) {
      this.ownerView._enum.appendChild(this._target);
      this.ownerView._enumItems.push(this);
    } else {
      this.ownerView._nonenum.appendChild(this._target);
      this.ownerView._nonEnumItems.push(this);
    }
  },

  /**
   * Creates the necessary nodes for this variable.
   */
  _displayVariable: function() {
    let document = this.document;
    let descriptor = this._initialDescriptor;

    let separatorLabel = this._separatorLabel = document.createElement("label");
    separatorLabel.className = "plain separator";
    separatorLabel.setAttribute("value", this.separatorStr + " ");

    let valueLabel = this._valueLabel = document.createElement("label");
    valueLabel.className = "plain value";
    valueLabel.setAttribute("flex", "1");
    valueLabel.setAttribute("crop", "center");

    this._title.appendChild(separatorLabel);
    this._title.appendChild(valueLabel);

    if (VariablesView.isPrimitive(descriptor)) {
      this.hideArrow();
    }

    // If no value will be displayed, we don't need the separator.
    if (!descriptor.get && !descriptor.set && !("value" in descriptor)) {
      separatorLabel.hidden = true;
    }

    // If this is a getter/setter property, create two child pseudo-properties
    // called "get" and "set" that display the corresponding functions.
    if (descriptor.get || descriptor.set) {
      separatorLabel.hidden = true;
      valueLabel.hidden = true;

      // Changing getter/setter names is never allowed.
      this.switch = null;

      // Getter/setter properties require special handling when it comes to
      // evaluation and deletion.
      if (this.ownerView.eval) {
        this.delete = VariablesView.getterOrSetterDeleteCallback;
        this.evaluationMacro = VariablesView.overrideValueEvalMacro;
      } else {
        // Deleting getters and setters individually is not allowed if no
        // evaluation method is provided.
        this.delete = null;
        this.evaluationMacro = null;
      }

      let getter = this.addItem("get", { value: descriptor.get });
      let setter = this.addItem("set", { value: descriptor.set });
      getter.evaluationMacro = VariablesView.getterOrSetterEvalMacro;
      setter.evaluationMacro = VariablesView.getterOrSetterEvalMacro;

      getter.hideArrow();
      setter.hideArrow();
      this.expand();
    }
  },

  /**
   * Adds specific nodes for this variable based on custom flags.
   */
  _customizeVariable: function() {
    let ownerView = this.ownerView;
    let descriptor = this._initialDescriptor;

    if (ownerView.eval && this.getter || this.setter) {
      let editNode = this._editNode = this.document.createElement("toolbarbutton");
      editNode.className = "plain variables-view-edit";
      editNode.addEventListener("mousedown", this._onEdit.bind(this));
      this._title.insertBefore(editNode, this._spacer);
    }

    if (ownerView.delete) {
      let deleteNode = this._deleteNode = this.document.createElement("toolbarbutton");
      deleteNode.className = "plain variables-view-delete";
      deleteNode.addEventListener("click", this._onDelete.bind(this));
      this._title.appendChild(deleteNode);
    }

    if (ownerView.new) {
      let addPropertyNode = this._addPropertyNode = this.document.createElement("toolbarbutton");
      addPropertyNode.className = "plain variables-view-add-property";
      addPropertyNode.addEventListener("mousedown", this._onAddProperty.bind(this));
      this._title.appendChild(addPropertyNode);

      // Can't add properties to primitive values, hide the node in those cases.
      if (VariablesView.isPrimitive(descriptor)) {
        addPropertyNode.setAttribute("invisible", "");
      }
    }

    if (ownerView.contextMenuId) {
      this._title.setAttribute("context", ownerView.contextMenuId);
    }

    if (ownerView.preventDescriptorModifiers) {
      return;
    }

    if (!descriptor.writable && !ownerView.getter && !ownerView.setter) {
      let nonWritableIcon = this.document.createElement("hbox");
      nonWritableIcon.className = "plain variable-or-property-non-writable-icon";
      nonWritableIcon.setAttribute("optional-visibility", "");
      this._title.appendChild(nonWritableIcon);
    }
    if (descriptor.value && typeof descriptor.value == "object") {
      if (descriptor.value.frozen) {
        let frozenLabel = this.document.createElement("label");
        frozenLabel.className = "plain variable-or-property-frozen-label";
        frozenLabel.setAttribute("optional-visibility", "");
        frozenLabel.setAttribute("value", "F");
        this._title.appendChild(frozenLabel);
      }
      if (descriptor.value.sealed) {
        let sealedLabel = this.document.createElement("label");
        sealedLabel.className = "plain variable-or-property-sealed-label";
        sealedLabel.setAttribute("optional-visibility", "");
        sealedLabel.setAttribute("value", "S");
        this._title.appendChild(sealedLabel);
      }
      if (!descriptor.value.extensible) {
        let nonExtensibleLabel = this.document.createElement("label");
        nonExtensibleLabel.className = "plain variable-or-property-non-extensible-label";
        nonExtensibleLabel.setAttribute("optional-visibility", "");
        nonExtensibleLabel.setAttribute("value", "N");
        this._title.appendChild(nonExtensibleLabel);
      }
    }
  },

  /**
   * Prepares all tooltips for this variable.
   */
  _prepareTooltips: function() {
    this._target.addEventListener("mouseover", this._setTooltips);
  },

  /**
   * Sets all tooltips for this variable.
   */
  _setTooltips: function() {
    this._target.removeEventListener("mouseover", this._setTooltips);

    let ownerView = this.ownerView;
    if (ownerView.preventDescriptorModifiers) {
      return;
    }

    let tooltip = this.document.createElement("tooltip");
    tooltip.id = "tooltip-" + this._idString;
    tooltip.setAttribute("orient", "horizontal");

    let labels = [
      "configurable", "enumerable", "writable",
      "frozen", "sealed", "extensible", "overridden", "WebIDL"];

    for (let type of labels) {
      let labelElement = this.document.createElement("label");
      labelElement.className = type;
      labelElement.setAttribute("value", L10N.getStr(type + "Tooltip"));
      tooltip.appendChild(labelElement);
    }

    this._target.appendChild(tooltip);
    this._target.setAttribute("tooltip", tooltip.id);

    if (this._editNode && ownerView.eval) {
      this._editNode.setAttribute("tooltiptext", ownerView.editButtonTooltip);
    }
    if (this._openInspectorNode && this._linkedToInspector) {
      this._openInspectorNode.setAttribute("tooltiptext", this.ownerView.domNodeValueTooltip);
    }
    if (this._valueLabel && ownerView.eval) {
      this._valueLabel.setAttribute("tooltiptext", ownerView.editableValueTooltip);
    }
    if (this._name && ownerView.switch) {
      this._name.setAttribute("tooltiptext", ownerView.editableNameTooltip);
    }
    if (this._deleteNode && ownerView.delete) {
      this._deleteNode.setAttribute("tooltiptext", ownerView.deleteButtonTooltip);
    }
  },

  /**
   * Get the parent variablesview toolbox, if any.
   */
  get toolbox() {
    return this._variablesView.toolbox;
  },

  /**
   * Checks if this variable is a DOMNode and is part of a variablesview that
   * has been linked to the toolbox, so that highlighting and jumping to the
   * inspector can be done.
   */
  _isLinkableToInspector: function() {
    let isDomNode = this._valueGrip && this._valueGrip.preview.kind === "DOMNode";
    let hasBeenLinked = this._linkedToInspector;
    let hasToolbox = !!this.toolbox;

    return isDomNode && !hasBeenLinked && hasToolbox;
  },

  /**
   * If the variable is a DOMNode, and if a toolbox is set, then link it to the
   * inspector (highlight on hover, and jump to markup-view on click)
   */
  _linkToInspector: function() {
    if (!this._isLinkableToInspector()) {
      return;
    }

    // Listen to value mouseover/click events to highlight and jump
    this._valueLabel.addEventListener("mouseover", this.highlightDomNode);
    this._valueLabel.addEventListener("mouseout", this.unhighlightDomNode);

    // Add a button to open the node in the inspector
    this._openInspectorNode = this.document.createElement("toolbarbutton");
    this._openInspectorNode.className = "plain variables-view-open-inspector";
    this._openInspectorNode.addEventListener("mousedown", this.openNodeInInspector);
    this._title.appendChild(this._openInspectorNode);

    this._linkedToInspector = true;
  },

  /**
   * In case this variable is a DOMNode and part of a variablesview that has been
   * linked to the toolbox's inspector, then select the corresponding node in
   * the inspector, and switch the inspector tool in the toolbox
   * @return a promise that resolves when the node is selected and the inspector
   * has been switched to and is ready
   */
  openNodeInInspector: function(event) {
    if (!this.toolbox) {
      return promise.reject(new Error("Toolbox not available"));
    }

    event && event.stopPropagation();

    return (async function() {
      await this.toolbox.initInspector();

      let nodeFront = this._nodeFront;
      if (!nodeFront) {
        nodeFront = await this.toolbox.walker.getNodeActorFromObjectActor(this._valueGrip.actor);
      }

      if (nodeFront) {
        await this.toolbox.selectTool("inspector");

        let inspectorReady = defer();
        this.toolbox.getPanel("inspector").once("inspector-updated", inspectorReady.resolve);
        await this.toolbox.selection.setNodeFront(nodeFront, { reason: "variables-view" });
        await inspectorReady.promise;
      }
    }.bind(this))();
  },

  /**
   * In case this variable is a DOMNode and part of a variablesview that has been
   * linked to the toolbox's inspector, then highlight the corresponding node
   */
  highlightDomNode: function() {
    if (this.toolbox) {
      if (this._nodeFront) {
        // If the nodeFront has been retrieved before, no need to ask the server
        // again for it
        this.toolbox.highlighterUtils.highlightNodeFront(this._nodeFront);
        return;
      }

      this.toolbox.highlighterUtils.highlightDomValueGrip(this._valueGrip).then(front => {
        this._nodeFront = front;
      });
    }
  },

  /**
   * Unhighlight a previously highlit node
   * @see highlightDomNode
   */
  unhighlightDomNode: function() {
    if (this.toolbox) {
      this.toolbox.highlighterUtils.unhighlight();
    }
  },

  /**
   * Sets a variable's configurable, enumerable and writable attributes,
   * and specifies if it's a 'this', '<exception>', '<return>' or '__proto__'
   * reference.
   */
  _setAttributes: function() {
    let ownerView = this.ownerView;
    if (ownerView.preventDescriptorModifiers) {
      return;
    }

    let descriptor = this._initialDescriptor;
    let target = this._target;
    let name = this._nameString;

    if (ownerView.eval) {
      target.setAttribute("editable", "");
    }

    if (!descriptor.configurable) {
      target.setAttribute("non-configurable", "");
    }
    if (!descriptor.enumerable) {
      target.setAttribute("non-enumerable", "");
    }
    if (!descriptor.writable && !ownerView.getter && !ownerView.setter) {
      target.setAttribute("non-writable", "");
    }

    if (descriptor.value && typeof descriptor.value == "object") {
      if (descriptor.value.frozen) {
        target.setAttribute("frozen", "");
      }
      if (descriptor.value.sealed) {
        target.setAttribute("sealed", "");
      }
      if (!descriptor.value.extensible) {
        target.setAttribute("non-extensible", "");
      }
    }

    if (descriptor && "getterValue" in descriptor) {
      target.setAttribute("safe-getter", "");
    }

    if (name == "this") {
      target.setAttribute("self", "");
    } else if (this._internalItem && name == "<exception>") {
      target.setAttribute("exception", "");
      target.setAttribute("pseudo-item", "");
    } else if (this._internalItem && name == "<return>") {
      target.setAttribute("return", "");
      target.setAttribute("pseudo-item", "");
    } else if (name == "__proto__") {
      target.setAttribute("proto", "");
      target.setAttribute("pseudo-item", "");
    }

    if (Object.keys(descriptor).length == 0) {
      target.setAttribute("pseudo-item", "");
    }
  },

  /**
   * Adds the necessary event listeners for this variable.
   */
  _addEventListeners: function() {
    this._name.addEventListener("dblclick", this._activateNameInput);
    this._valueLabel.addEventListener("mousedown", this._activateValueInput);
    this._title.addEventListener("mousedown", this._onClick);
  },

  /**
   * Makes this variable's name editable.
   */
  _activateNameInput: function(e) {
    if (!this._variablesView.alignedValues) {
      this._separatorLabel.hidden = true;
      this._valueLabel.hidden = true;
    }

    EditableName.create(this, {
      onSave: aKey => {
        if (!this._variablesView.preventDisableOnChange) {
          this._disable();
        }
        this.ownerView.switch(this, aKey);
      },
      onCleanup: () => {
        if (!this._variablesView.alignedValues) {
          this._separatorLabel.hidden = false;
          this._valueLabel.hidden = false;
        }
      }
    }, e);
  },

  /**
   * Makes this variable's value editable.
   */
  _activateValueInput: function(e) {
    EditableValue.create(this, {
      onSave: aString => {
        if (this._linkedToInspector) {
          this.unhighlightDomNode();
        }
        if (!this._variablesView.preventDisableOnChange) {
          this._disable();
        }
        this.ownerView.eval(this, aString);
      }
    }, e);
  },

  /**
   * Disables this variable prior to a new name switch or value evaluation.
   */
  _disable: function() {
    // Prevent the variable from being collapsed or expanded.
    this.hideArrow();

    // Hide any nodes that may offer information about the variable.
    for (let node of this._title.childNodes) {
      node.hidden = node != this._arrow && node != this._name;
    }
    this._enum.hidden = true;
    this._nonenum.hidden = true;
  },

  /**
   * The current macro used to generate the string evaluated when performing
   * a variable or property value change.
   */
  evaluationMacro: VariablesView.simpleValueEvalMacro,

  /**
   * The click listener for the edit button.
   */
  _onEdit: function(e) {
    if (e.button != 0) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    this._activateValueInput();
  },

  /**
   * The click listener for the delete button.
   */
  _onDelete: function(e) {
    if ("button" in e && e.button != 0) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    if (this.ownerView.delete) {
      if (!this.ownerView.delete(this)) {
        this.hide();
      }
    }
  },

  /**
   * The click listener for the add property button.
   */
  _onAddProperty: function(e) {
    if ("button" in e && e.button != 0) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    this.expanded = true;

    let item = this.addItem(" ", {
      value: undefined,
      configurable: true,
      enumerable: true,
      writable: true
    }, {relaxed: true});

    // Force showing the separator.
    item._separatorLabel.hidden = false;

    EditableNameAndValue.create(item, {
      onSave: ([aKey, aValue]) => {
        if (!this._variablesView.preventDisableOnChange) {
          this._disable();
        }
        this.ownerView.new(this, aKey, aValue);
      }
    }, e);
  },

  _symbolicName: null,
  _symbolicPath: null,
  _absoluteName: null,
  _initialDescriptor: null,
  _separatorLabel: null,
  _valueLabel: null,
  _spacer: null,
  _editNode: null,
  _deleteNode: null,
  _addPropertyNode: null,
  _tooltip: null,
  _valueGrip: null,
  _valueString: "",
  _valueClassName: "",
  _prevExpandable: false,
  _prevExpanded: false
});

/**
 * A Property is a Variable holding additional child Property instances.
 * Iterable via "for (let [name, property] of instance) { }".
 *
 * @param Variable aVar
 *        The variable to contain this property.
 * @param string aName
 *        The property's name.
 * @param object aDescriptor
 *        The property's descriptor.
 * @param object aOptions
 *        Options of the form accepted by Scope.addItem
 */
function Property(aVar, aName, aDescriptor, aOptions) {
  Variable.call(this, aVar, aName, aDescriptor, aOptions);
}

Property.prototype = extend(Variable.prototype, {
  /**
   * The class name applied to this property's target element.
   */
  targetClassName: "variables-view-property variable-or-property",

  /**
   * @see Variable.symbolicName
   * @return string
   */
  get symbolicName() {
    if (this._symbolicName) {
      return this._symbolicName;
    }

    this._symbolicName = this.ownerView.symbolicName + "[" + escapeString(this._nameString) + "]";
    return this._symbolicName;
  },

  /**
   * @see Variable.absoluteName
   * @return string
   */
  get absoluteName() {
    if (this._absoluteName) {
      return this._absoluteName;
    }

    this._absoluteName = this.ownerView.absoluteName + "[" + escapeString(this._nameString) + "]";
    return this._absoluteName;
  }
});

/**
 * A generator-iterator over the VariablesView, Scopes, Variables and Properties.
 */
VariablesView.prototype[Symbol.iterator] =
Scope.prototype[Symbol.iterator] =
Variable.prototype[Symbol.iterator] =
Property.prototype[Symbol.iterator] = function* () {
  yield* this._store;
};

/**
 * Forget everything recorded about added scopes, variables or properties.
 * @see VariablesView.commitHierarchy
 */
VariablesView.prototype.clearHierarchy = function() {
  this._prevHierarchy.clear();
  this._currHierarchy.clear();
};

/**
 * Perform operations on all the VariablesView Scopes, Variables and Properties
 * after you've added all the items you wanted.
 *
 * Calling this method is optional, and does the following:
 *   - styles the items overridden by other items in parent scopes
 *   - reopens the items which were previously expanded
 *   - flashes the items whose values changed
 */
VariablesView.prototype.commitHierarchy = function() {
  for (let [, currItem] of this._currHierarchy) {
    // Avoid performing expensive operations.
    if (this.commitHierarchyIgnoredItems[currItem._nameString]) {
      continue;
    }
    let overridden = this.isOverridden(currItem);
    if (overridden) {
      currItem.setOverridden(true);
    }
    let expanded = !currItem._committed && this.wasExpanded(currItem);
    if (expanded) {
      currItem.expand();
    }
    let changed = !currItem._committed && this.hasChanged(currItem);
    if (changed) {
      currItem.flash();
    }
    currItem._committed = true;
  }
  if (this.oncommit) {
    this.oncommit(this);
  }
};

// Some variables are likely to contain a very large number of properties.
// It would be a bad idea to re-expand them or perform expensive operations.
VariablesView.prototype.commitHierarchyIgnoredItems = extend(null, {
  "window": true,
  "this": true
});

/**
 * Checks if the an item was previously expanded, if it existed in a
 * previous hierarchy.
 *
 * @param Scope | Variable | Property aItem
 *        The item to verify.
 * @return boolean
 *         Whether the item was expanded.
 */
VariablesView.prototype.wasExpanded = function(aItem) {
  if (!(aItem instanceof Scope)) {
    return false;
  }
  let prevItem = this._prevHierarchy.get(aItem.absoluteName || aItem._nameString);
  return prevItem ? prevItem._isExpanded : false;
};

/**
 * Checks if the an item's displayed value (a representation of the grip)
 * has changed, if it existed in a previous hierarchy.
 *
 * @param Variable | Property aItem
 *        The item to verify.
 * @return boolean
 *         Whether the item has changed.
 */
VariablesView.prototype.hasChanged = function(aItem) {
  // Only analyze Variables and Properties for displayed value changes.
  // Scopes are just collections of Variables and Properties and
  // don't have a "value", so they can't change.
  if (!(aItem instanceof Variable)) {
    return false;
  }
  let prevItem = this._prevHierarchy.get(aItem.absoluteName);
  return prevItem ? prevItem._valueString != aItem._valueString : false;
};

/**
 * Checks if the an item was previously expanded, if it existed in a
 * previous hierarchy.
 *
 * @param Scope | Variable | Property aItem
 *        The item to verify.
 * @return boolean
 *         Whether the item was expanded.
 */
VariablesView.prototype.isOverridden = function(aItem) {
  // Only analyze Variables for being overridden in different Scopes.
  if (!(aItem instanceof Variable) || aItem instanceof Property) {
    return false;
  }
  let currVariableName = aItem._nameString;
  let parentScopes = this.getParentScopesForVariableOrProperty(aItem);

  for (let otherScope of parentScopes) {
    for (let [otherVariableName] of otherScope) {
      if (otherVariableName == currVariableName) {
        return true;
      }
    }
  }
  return false;
};

/**
 * Returns true if the descriptor represents an undefined, null or
 * primitive value.
 *
 * @param object aDescriptor
 *        The variable's descriptor.
 */
VariablesView.isPrimitive = function(aDescriptor) {
  // For accessor property descriptors, the getter and setter need to be
  // contained in 'get' and 'set' properties.
  let getter = aDescriptor.get;
  let setter = aDescriptor.set;
  if (getter || setter) {
    return false;
  }

  // As described in the remote debugger protocol, the value grip
  // must be contained in a 'value' property.
  let grip = aDescriptor.value;
  if (typeof grip != "object") {
    return true;
  }

  // For convenience, undefined, null, Infinity, -Infinity, NaN, -0, and long
  // strings are considered types.
  let type = grip.type;
  if (type == "undefined" ||
      type == "null" ||
      type == "Infinity" ||
      type == "-Infinity" ||
      type == "NaN" ||
      type == "-0" ||
      type == "symbol" ||
      type == "longString") {
    return true;
  }

  return false;
};

/**
 * Returns true if the descriptor represents an undefined value.
 *
 * @param object aDescriptor
 *        The variable's descriptor.
 */
VariablesView.isUndefined = function(aDescriptor) {
  // For accessor property descriptors, the getter and setter need to be
  // contained in 'get' and 'set' properties.
  let getter = aDescriptor.get;
  let setter = aDescriptor.set;
  if (typeof getter == "object" && getter.type == "undefined" &&
      typeof setter == "object" && setter.type == "undefined") {
    return true;
  }

  // As described in the remote debugger protocol, the value grip
  // must be contained in a 'value' property.
  let grip = aDescriptor.value;
  if (typeof grip == "object" && grip.type == "undefined") {
    return true;
  }

  return false;
};

/**
 * Returns true if the descriptor represents a falsy value.
 *
 * @param object aDescriptor
 *        The variable's descriptor.
 */
VariablesView.isFalsy = function(aDescriptor) {
  // As described in the remote debugger protocol, the value grip
  // must be contained in a 'value' property.
  let grip = aDescriptor.value;
  if (typeof grip != "object") {
    return !grip;
  }

  // For convenience, undefined, null, NaN, and -0 are all considered types.
  let type = grip.type;
  if (type == "undefined" ||
      type == "null" ||
      type == "NaN" ||
      type == "-0") {
    return true;
  }

  return false;
};

/**
 * Returns true if the value is an instance of Variable or Property.
 *
 * @param any aValue
 *        The value to test.
 */
VariablesView.isVariable = function(aValue) {
  return aValue instanceof Variable;
};

/**
 * Returns a standard grip for a value.
 *
 * @param any aValue
 *        The raw value to get a grip for.
 * @return any
 *         The value's grip.
 */
VariablesView.getGrip = function(aValue) {
  switch (typeof aValue) {
    case "boolean":
    case "string":
      return aValue;
    case "number":
      if (aValue === Infinity) {
        return { type: "Infinity" };
      } else if (aValue === -Infinity) {
        return { type: "-Infinity" };
      } else if (Number.isNaN(aValue)) {
        return { type: "NaN" };
      } else if (1 / aValue === -Infinity) {
        return { type: "-0" };
      }
      return aValue;
    case "undefined":
      // document.all is also "undefined"
      if (aValue === undefined) {
        return { type: "undefined" };
      }
      // fall through
    case "object":
      if (aValue === null) {
        return { type: "null" };
      }
      // fall through
    case "function":
      return { type: "object",
               class: WebConsoleUtils.getObjectClassName(aValue) };
    default:
      console.error("Failed to provide a grip for value of " + typeof value +
                    ": " + aValue);
      return null;
  }
};

/**
 * Returns a custom formatted property string for a grip.
 *
 * @param any aGrip
 *        @see Variable.setGrip
 * @param object aOptions
 *        Options:
 *        - concise: boolean that tells you want a concisely formatted string.
 *        - noStringQuotes: boolean that tells to not quote strings.
 *        - noEllipsis: boolean that tells to not add an ellipsis after the
 *        initial text of a longString.
 * @return string
 *         The formatted property string.
 */
VariablesView.getString = function(aGrip, aOptions = {}) {
  if (aGrip && typeof aGrip == "object") {
    switch (aGrip.type) {
      case "undefined":
      case "null":
      case "NaN":
      case "Infinity":
      case "-Infinity":
      case "-0":
        return aGrip.type;
      default:
        let stringifier = VariablesView.stringifiers.byType[aGrip.type];
        if (stringifier) {
          let result = stringifier(aGrip, aOptions);
          if (result != null) {
            return result;
          }
        }

        if (aGrip.displayString) {
          return VariablesView.getString(aGrip.displayString, aOptions);
        }

        if (aGrip.type == "object" && aOptions.concise) {
          return aGrip.class;
        }

        return "[" + aGrip.type + " " + aGrip.class + "]";
    }
  }

  switch (typeof aGrip) {
    case "string":
      return VariablesView.stringifiers.byType.string(aGrip, aOptions);
    case "boolean":
      return aGrip ? "true" : "false";
    case "number":
      if (!aGrip && 1 / aGrip === -Infinity) {
        return "-0";
      }
      // fall through
    default:
      return aGrip + "";
  }
};

/**
 * The VariablesView stringifiers are used by VariablesView.getString(). These
 * are organized by object type, object class and by object actor preview kind.
 * Some objects share identical ways for previews, for example Arrays, Sets and
 * NodeLists.
 *
 * Any stringifier function must return a string. If null is returned, * then
 * the default stringifier will be used. When invoked, the stringifier is
 * given the same two arguments as those given to VariablesView.getString().
 */
VariablesView.stringifiers = {};

VariablesView.stringifiers.byType = {
  string: function(aGrip, {noStringQuotes}) {
    if (noStringQuotes) {
      return aGrip;
    }
    return '"' + aGrip + '"';
  },

  longString: function({initial}, {noStringQuotes, noEllipsis}) {
    let ellipsis = noEllipsis ? "" : ELLIPSIS;
    if (noStringQuotes) {
      return initial + ellipsis;
    }
    let result = '"' + initial + '"';
    if (!ellipsis) {
      return result;
    }
    return result.substr(0, result.length - 1) + ellipsis + '"';
  },

  object: function(aGrip, aOptions) {
    let {preview} = aGrip;
    let stringifier;
    if (aGrip.class) {
      stringifier = VariablesView.stringifiers.byObjectClass[aGrip.class];
    }
    if (!stringifier && preview && preview.kind) {
      stringifier = VariablesView.stringifiers.byObjectKind[preview.kind];
    }
    if (stringifier) {
      return stringifier(aGrip, aOptions);
    }
    return null;
  },

  symbol: function(aGrip, aOptions) {
    const name = aGrip.name || "";
    return "Symbol(" + name + ")";
  },

  mapEntry: function(aGrip, {concise}) {
    let { preview: { key, value }} = aGrip;

    let keyString = VariablesView.getString(key, {
      concise: true,
      noStringQuotes: true,
    });
    let valueString = VariablesView.getString(value, { concise: true });

    return keyString + " \u2192 " + valueString;
  },

}; // VariablesView.stringifiers.byType

VariablesView.stringifiers.byObjectClass = {
  Function: function(aGrip, {concise}) {
    // TODO: Bug 948484 - support arrow functions and ES6 generators

    let name = aGrip.userDisplayName || aGrip.displayName || aGrip.name || "";
    name = VariablesView.getString(name, { noStringQuotes: true });

    // TODO: Bug 948489 - Support functions with destructured parameters and
    // rest parameters
    let params = aGrip.parameterNames || "";
    if (!concise) {
      return "function " + name + "(" + params + ")";
    }
    return (name || "function ") + "(" + params + ")";
  },

  RegExp: function({displayString}) {
    return VariablesView.getString(displayString, { noStringQuotes: true });
  },

  Date: function({preview}) {
    if (!preview || !("timestamp" in preview)) {
      return null;
    }

    if (typeof preview.timestamp != "number") {
      return new Date(preview.timestamp).toString(); // invalid date
    }

    return "Date " + new Date(preview.timestamp).toISOString();
  },

  Number: function(aGrip) {
    let {preview} = aGrip;
    if (preview === undefined) {
      return null;
    }
    return aGrip.class + " { " + VariablesView.getString(preview.wrappedValue) +
      " }";
  },
}; // VariablesView.stringifiers.byObjectClass

VariablesView.stringifiers.byObjectClass.Boolean =
  VariablesView.stringifiers.byObjectClass.Number;

VariablesView.stringifiers.byObjectKind = {
  ArrayLike: function(aGrip, {concise}) {
    let {preview} = aGrip;
    if (concise) {
      return aGrip.class + "[" + preview.length + "]";
    }

    if (!preview.items) {
      return null;
    }

    let shown = 0, result = [], lastHole = null;
    for (let item of preview.items) {
      if (item === null) {
        if (lastHole !== null) {
          result[lastHole] += ",";
        } else {
          result.push("");
        }
        lastHole = result.length - 1;
      } else {
        lastHole = null;
        result.push(VariablesView.getString(item, { concise: true }));
      }
      shown++;
    }

    if (shown < preview.length) {
      let n = preview.length - shown;
      result.push(VariablesView.stringifiers._getNMoreString(n));
    } else if (lastHole !== null) {
      // make sure we have the right number of commas...
      result[lastHole] += ",";
    }

    let prefix = aGrip.class == "Array" ? "" : aGrip.class + " ";
    return prefix + "[" + result.join(", ") + "]";
  },

  MapLike: function(aGrip, {concise}) {
    let {preview} = aGrip;
    if (concise || !preview.entries) {
      let size = typeof preview.size == "number" ?
                   "[" + preview.size + "]" : "";
      return aGrip.class + size;
    }

    let entries = [];
    for (let [key, value] of preview.entries) {
      let keyString = VariablesView.getString(key, {
        concise: true,
        noStringQuotes: true,
      });
      let valueString = VariablesView.getString(value, { concise: true });
      entries.push(keyString + ": " + valueString);
    }

    if (typeof preview.size == "number" && preview.size > entries.length) {
      let n = preview.size - entries.length;
      entries.push(VariablesView.stringifiers._getNMoreString(n));
    }

    return aGrip.class + " {" + entries.join(", ") + "}";
  },

  ObjectWithText: function(aGrip, {concise}) {
    if (concise) {
      return aGrip.class;
    }

    return aGrip.class + " " + VariablesView.getString(aGrip.preview.text);
  },

  ObjectWithURL: function(aGrip, {concise}) {
    let result = aGrip.class;
    let url = aGrip.preview.url;
    if (!VariablesView.isFalsy({ value: url })) {
      result += ` \u2192 ${getSourceNames(url)[concise ? "short" : "long"]}`;
    }
    return result;
  },

  // Stringifier for any kind of object.
  Object: function(aGrip, {concise}) {
    if (concise) {
      return aGrip.class;
    }

    let {preview} = aGrip;
    let props = [];

    if (aGrip.class == "Promise" && aGrip.promiseState) {
      let { state, value, reason } = aGrip.promiseState;
      props.push("<state>: " + VariablesView.getString(state));
      if (state == "fulfilled") {
        props.push("<value>: " + VariablesView.getString(value, { concise: true }));
      } else if (state == "rejected") {
        props.push("<reason>: " + VariablesView.getString(reason, { concise: true }));
      }
    }

    for (let key of Object.keys(preview.ownProperties || {})) {
      let value = preview.ownProperties[key];
      let valueString = "";
      if (value.get) {
        valueString = "Getter";
      } else if (value.set) {
        valueString = "Setter";
      } else {
        valueString = VariablesView.getString(value.value, { concise: true });
      }
      props.push(key + ": " + valueString);
    }

    for (let key of Object.keys(preview.safeGetterValues || {})) {
      let value = preview.safeGetterValues[key];
      let valueString = VariablesView.getString(value.getterValue,
                                                { concise: true });
      props.push(key + ": " + valueString);
    }

    if (!props.length) {
      return null;
    }

    if (preview.ownPropertiesLength) {
      let previewLength = Object.keys(preview.ownProperties).length;
      let diff = preview.ownPropertiesLength - previewLength;
      if (diff > 0) {
        props.push(VariablesView.stringifiers._getNMoreString(diff));
      }
    }

    let prefix = aGrip.class != "Object" ? aGrip.class + " " : "";
    return prefix + "{" + props.join(", ") + "}";
  }, // Object

  Error: function(aGrip, {concise}) {
    let {preview} = aGrip;
    let name = VariablesView.getString(preview.name, { noStringQuotes: true });
    if (concise) {
      return name || aGrip.class;
    }

    let msg = name + ": " +
              VariablesView.getString(preview.message, { noStringQuotes: true });

    if (!VariablesView.isFalsy({ value: preview.stack })) {
      msg += "\n" + L10N.getStr("variablesViewErrorStacktrace") +
             "\n" + preview.stack;
    }

    return msg;
  },

  DOMException: function(aGrip, {concise}) {
    let {preview} = aGrip;
    if (concise) {
      return preview.name || aGrip.class;
    }

    let msg = aGrip.class + " [" + preview.name + ": " +
              VariablesView.getString(preview.message) + "\n" +
              "code: " + preview.code + "\n" +
              "nsresult: 0x" + (+preview.result).toString(16);

    if (preview.filename) {
      msg += "\nlocation: " + preview.filename;
      if (preview.lineNumber) {
        msg += ":" + preview.lineNumber;
      }
    }

    return msg + "]";
  },

  DOMEvent: function(aGrip, {concise}) {
    let {preview} = aGrip;
    if (!preview.type) {
      return null;
    }

    if (concise) {
      return aGrip.class + " " + preview.type;
    }

    let result = preview.type;

    if (preview.eventKind == "key" && preview.modifiers &&
        preview.modifiers.length) {
      result += " " + preview.modifiers.join("-");
    }

    let props = [];
    if (preview.target) {
      let target = VariablesView.getString(preview.target, { concise: true });
      props.push("target: " + target);
    }

    for (let prop in preview.properties) {
      let value = preview.properties[prop];
      props.push(prop + ": " + VariablesView.getString(value, { concise: true }));
    }

    return result + " {" + props.join(", ") + "}";
  }, // DOMEvent

  DOMNode: function(aGrip, {concise}) {
    let {preview} = aGrip;

    switch (preview.nodeType) {
      case nodeConstants.DOCUMENT_NODE: {
        let result = aGrip.class;
        if (preview.location) {
          result += ` \u2192 ${getSourceNames(preview.location)[concise ? "short" : "long"]}`;
        }

        return result;
      }

      case nodeConstants.ATTRIBUTE_NODE: {
        let value = VariablesView.getString(preview.value, { noStringQuotes: true });
        return preview.nodeName + '="' + escapeHTML(value) + '"';
      }

      case nodeConstants.TEXT_NODE:
        return preview.nodeName + " " +
               VariablesView.getString(preview.textContent);

      case nodeConstants.COMMENT_NODE: {
        let comment = VariablesView.getString(preview.textContent,
                                              { noStringQuotes: true });
        return "<!--" + comment + "-->";
      }

      case nodeConstants.DOCUMENT_FRAGMENT_NODE: {
        if (concise || !preview.childNodes) {
          return aGrip.class + "[" + preview.childNodesLength + "]";
        }
        let nodes = [];
        for (let node of preview.childNodes) {
          nodes.push(VariablesView.getString(node));
        }
        if (nodes.length < preview.childNodesLength) {
          let n = preview.childNodesLength - nodes.length;
          nodes.push(VariablesView.stringifiers._getNMoreString(n));
        }
        return aGrip.class + " [" + nodes.join(", ") + "]";
      }

      case nodeConstants.ELEMENT_NODE: {
        let attrs = preview.attributes;
        if (!concise) {
          let n = 0, result = "<" + preview.nodeName;
          for (let name in attrs) {
            let value = VariablesView.getString(attrs[name],
                                                { noStringQuotes: true });
            result += " " + name + '="' + escapeHTML(value) + '"';
            n++;
          }
          if (preview.attributesLength > n) {
            result += " " + ELLIPSIS;
          }
          return result + ">";
        }

        let result = "<" + preview.nodeName;
        if (attrs.id) {
          result += "#" + attrs.id;
        }

        if (attrs.class) {
          result += "." + attrs.class.trim().replace(/\s+/, ".");
        }
        return result + ">";
      }

      default:
        return null;
    }
  }, // DOMNode
}; // VariablesView.stringifiers.byObjectKind

/**
 * Get the "N more…" formatted string, given an N. This is used for displaying
 * how many elements are not displayed in an object preview (eg. an array).
 *
 * @private
 * @param number aNumber
 * @return string
 */
VariablesView.stringifiers._getNMoreString = function(aNumber) {
  let str = L10N.getStr("variablesViewMoreObjects");
  return PluralForm.get(aNumber, str).replace("#1", aNumber);
};

/**
 * Returns a custom class style for a grip.
 *
 * @param any aGrip
 *        @see Variable.setGrip
 * @return string
 *         The custom class style.
 */
VariablesView.getClass = function(aGrip) {
  if (aGrip && typeof aGrip == "object") {
    if (aGrip.preview) {
      switch (aGrip.preview.kind) {
        case "DOMNode":
          return "token-domnode";
      }
    }

    switch (aGrip.type) {
      case "undefined":
        return "token-undefined";
      case "null":
        return "token-null";
      case "Infinity":
      case "-Infinity":
      case "NaN":
      case "-0":
        return "token-number";
      case "longString":
        return "token-string";
    }
  }
  switch (typeof aGrip) {
    case "string":
      return "token-string";
    case "boolean":
      return "token-boolean";
    case "number":
      return "token-number";
    default:
      return "token-other";
  }
};

/**
 * A monotonically-increasing counter, that guarantees the uniqueness of scope,
 * variables and properties ids.
 *
 * @param string aName
 *        An optional string to prefix the id with.
 * @return number
 *         A unique id.
 */
var generateId = (function() {
  let count = 0;
  return function(aName = "") {
    return aName.toLowerCase().trim().replace(/\s+/g, "-") + (++count);
  };
})();

/**
 * Quote and escape a string. The result will be another string containing an
 * ECMAScript StringLiteral which will produce the original one when evaluated
 * by `eval` or similar.
 *
 * @param string aString
 *       An optional string to be escaped. If no string is passed, the function
 *       returns an empty string.
 * @return string
 */
function escapeString(aString) {
  if (typeof aString !== "string") {
    return "";
  }
  // U+2028 and U+2029 are allowed in JSON but not in ECMAScript string literals.
  return JSON.stringify(aString).replace(/\u2028/g, "\\u2028")
                                .replace(/\u2029/g, "\\u2029");
}

/**
 * Escape some HTML special characters. We do not need full HTML serialization
 * here, we just want to make strings safe to display in HTML attributes, for
 * the stringifiers.
 *
 * @param string aString
 * @return string
 */
function escapeHTML(aString) {
  return aString.replace(/&/g, "&amp;")
                .replace(/"/g, "&quot;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;");
}

/**
 * An Editable encapsulates the UI of an edit box that overlays a label,
 * allowing the user to edit the value.
 *
 * @param Variable aVariable
 *        The Variable or Property to make editable.
 * @param object aOptions
 *        - onSave
 *          The callback to call with the value when editing is complete.
 *        - onCleanup
 *          The callback to call when the editable is removed for any reason.
 */
function Editable(aVariable, aOptions) {
  this._variable = aVariable;
  this._onSave = aOptions.onSave;
  this._onCleanup = aOptions.onCleanup;
}

Editable.create = function(aVariable, aOptions, aEvent) {
  let editable = new this(aVariable, aOptions);
  editable.activate(aEvent);
  return editable;
};

Editable.prototype = {
  /**
   * The class name for targeting this Editable type's label element. Overridden
   * by inheriting classes.
   */
  className: null,

  /**
   * Boolean indicating whether this Editable should activate. Overridden by
   * inheriting classes.
   */
  shouldActivate: null,

  /**
   * The label element for this Editable. Overridden by inheriting classes.
   */
  label: null,

  /**
   * Activate this editable by replacing the input box it overlays and
   * initialize the handlers.
   *
   * @param Event e [optional]
   *        Optionally, the Event object that was used to activate the Editable.
   */
  activate: function(e) {
    if (!this.shouldActivate) {
      this._onCleanup && this._onCleanup();
      return;
    }

    let { label } = this;
    let initialString = label.getAttribute("value");

    if (e) {
      e.preventDefault();
      e.stopPropagation();
    }

    // Create a texbox input element which will be shown in the current
    // element's specified label location.
    let input = this._input = this._variable.document.createElement("textbox");
    input.className = "plain " + this.className;
    input.setAttribute("value", initialString);
    input.setAttribute("flex", "1");

    // Replace the specified label with a textbox input element.
    label.parentNode.replaceChild(input, label);
    this._variable._variablesView.boxObject.ensureElementIsVisible(input);
    input.select();

    // When the value is a string (displayed as "value"), then we probably want
    // to change it to another string in the textbox, so to avoid typing the ""
    // again, tackle with the selection bounds just a bit.
    if (initialString.match(/^".+"$/)) {
      input.selectionEnd--;
      input.selectionStart++;
    }

    this._onKeydown = this._onKeydown.bind(this);
    this._onBlur = this._onBlur.bind(this);
    input.addEventListener("keydown", this._onKeydown);
    input.addEventListener("blur", this._onBlur);

    this._prevExpandable = this._variable.twisty;
    this._prevExpanded = this._variable.expanded;
    this._variable.collapse();
    this._variable.hideArrow();
    this._variable.locked = true;
    this._variable.editing = true;
  },

  /**
   * Remove the input box and restore the Variable or Property to its previous
   * state.
   */
  deactivate: function() {
    this._input.removeEventListener("keydown", this._onKeydown);
    this._input.removeEventListener("blur", this.deactivate);
    this._input.parentNode.replaceChild(this.label, this._input);
    this._input = null;

    let { boxObject } = this._variable._variablesView;
    boxObject.scrollBy(-this._variable._target, 0);
    this._variable.locked = false;
    this._variable.twisty = this._prevExpandable;
    this._variable.expanded = this._prevExpanded;
    this._variable.editing = false;
    this._onCleanup && this._onCleanup();
  },

  /**
   * Save the current value and deactivate the Editable.
   */
  _save: function() {
    let initial = this.label.getAttribute("value");
    let current = this._input.value.trim();
    this.deactivate();
    if (initial != current) {
      this._onSave(current);
    }
  },

  /**
   * Called when tab is pressed, allowing subclasses to link different
   * behavior to tabbing if desired.
   */
  _next: function() {
    this._save();
  },

  /**
   * Called when escape is pressed, indicating a cancelling of editing without
   * saving.
   */
  _reset: function() {
    this.deactivate();
    this._variable.focus();
  },

  /**
   * Event handler for when the input loses focus.
   */
  _onBlur: function() {
    this.deactivate();
  },

  /**
   * Event handler for when the input receives a key press.
   */
  _onKeydown: function(e) {
    e.stopPropagation();

    switch (e.keyCode) {
      case KeyCodes.DOM_VK_TAB:
        this._next();
        break;
      case KeyCodes.DOM_VK_RETURN:
        this._save();
        break;
      case KeyCodes.DOM_VK_ESCAPE:
        this._reset();
        break;
    }
  },
};

/**
 * An Editable specific to editing the name of a Variable or Property.
 */
function EditableName(aVariable, aOptions) {
  Editable.call(this, aVariable, aOptions);
}

EditableName.create = Editable.create;

EditableName.prototype = extend(Editable.prototype, {
  className: "element-name-input",

  get label() {
    return this._variable._name;
  },

  get shouldActivate() {
    return !!this._variable.ownerView.switch;
  },
});

/**
 * An Editable specific to editing the value of a Variable or Property.
 */
function EditableValue(aVariable, aOptions) {
  Editable.call(this, aVariable, aOptions);
}

EditableValue.create = Editable.create;

EditableValue.prototype = extend(Editable.prototype, {
  className: "element-value-input",

  get label() {
    return this._variable._valueLabel;
  },

  get shouldActivate() {
    return !!this._variable.ownerView.eval;
  },
});

/**
 * An Editable specific to editing the key and value of a new property.
 */
function EditableNameAndValue(aVariable, aOptions) {
  EditableName.call(this, aVariable, aOptions);
}

EditableNameAndValue.create = Editable.create;

EditableNameAndValue.prototype = extend(EditableName.prototype, {
  _reset: function(e) {
    // Hide the Variable or Property if the user presses escape.
    this._variable.remove();
    this.deactivate();
  },

  _next: function(e) {
    // Override _next so as to set both key and value at the same time.
    let key = this._input.value;
    this.label.setAttribute("value", key);

    let valueEditable = EditableValue.create(this._variable, {
      onSave: aValue => {
        this._onSave([key, aValue]);
      }
    });
    valueEditable._reset = () => {
      this._variable.remove();
      valueEditable.deactivate();
    };
  },

  _save: function(e) {
    // Both _save and _next activate the value edit box.
    this._next(e);
  }
});
