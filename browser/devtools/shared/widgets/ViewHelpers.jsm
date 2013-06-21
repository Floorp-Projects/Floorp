/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const PANE_APPEARANCE_DELAY = 50;
const PAGE_SIZE_ITEM_COUNT_RATIO = 5;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["Heritage", "ViewHelpers", "WidgetMethods"];

/**
 * Inheritance helpers from the addon SDK's core/heritage.
 * Remove these when all devtools are loadered.
 */
this.Heritage = {
  /**
   * @see extend in sdk/core/heritage.
   */
  extend: function(aPrototype, aProperties = {}) {
    return Object.create(aPrototype, this.getOwnPropertyDescriptors(aProperties));
  },

  /**
   * @see getOwnPropertyDescriptors in sdk/core/heritage.
   */
  getOwnPropertyDescriptors: function(aObject) {
    return Object.getOwnPropertyNames(aObject).reduce((aDescriptor, aName) => {
      aDescriptor[aName] = Object.getOwnPropertyDescriptor(aObject, aName);
      return aDescriptor;
    }, {});
  }
};

/**
 * Helpers for creating and messaging between UI components.
 */
this.ViewHelpers = {
  /**
   * Convenience method, dispatching a custom event.
   *
   * @param nsIDOMNode aTarget
   *        A custom target element to dispatch the event from.
   * @param string aType
   *        The name of the event.
   * @param any aDetail
   *        The data passed when initializing the event.
   * @return boolean
   *         True if the event was cancelled or a registered handler
   *         called preventDefault.
   */
  dispatchEvent: function(aTarget, aType, aDetail) {
    if (!(aTarget instanceof Ci.nsIDOMNode)) {
      return true; // Event cancelled.
    }
    let document = aTarget.ownerDocument || aTarget;
    let dispatcher = aTarget.ownerDocument ? aTarget : document.documentElement;

    let event = document.createEvent("CustomEvent");
    event.initCustomEvent(aType, true, true, aDetail);
    return dispatcher.dispatchEvent(event);
  },

  /**
   * Helper delegating some of the DOM attribute methods of a node to a widget.
   *
   * @param object aWidget
   *        The widget to assign the methods to.
   * @param nsIDOMNode aNode
   *        A node to delegate the methods to.
   */
  delegateWidgetAttributeMethods: function(aWidget, aNode) {
    aWidget.getAttribute = aNode.getAttribute.bind(aNode);
    aWidget.setAttribute = aNode.setAttribute.bind(aNode);
    aWidget.removeAttribute = aNode.removeAttribute.bind(aNode);
  },

  /**
   * Helper delegating some of the DOM event methods of a node to a widget.
   *
   * @param object aWidget
   *        The widget to assign the methods to.
   * @param nsIDOMNode aNode
   *        A node to delegate the methods to.
   */
  delegateWidgetEventMethods: function(aWidget, aNode) {
    aWidget.addEventListener = aNode.addEventListener.bind(aNode);
    aWidget.removeEventListener = aNode.removeEventListener.bind(aNode);
  },

  /**
   * Checks if the specified object looks like it's been decorated by an
   * event emitter.
   *
   * @return boolean
   *         True if it looks, walks and quacks like an event emitter.
   */
  isEventEmitter: function(aObject) {
    return aObject && aObject.on && aObject.off && aObject.once && aObject.emit;
  },

  /**
   * Prevents event propagation when navigation keys are pressed.
   *
   * @param Event e
   *        The event to be prevented.
   */
  preventScrolling: function(e) {
    switch (e.keyCode) {
      case e.DOM_VK_UP:
      case e.DOM_VK_DOWN:
      case e.DOM_VK_LEFT:
      case e.DOM_VK_RIGHT:
      case e.DOM_VK_PAGE_UP:
      case e.DOM_VK_PAGE_DOWN:
      case e.DOM_VK_HOME:
      case e.DOM_VK_END:
        e.preventDefault();
        e.stopPropagation();
    }
  },

  /**
   * Sets a side pane hidden or visible.
   *
   * @param object aFlags
   *        An object containing some of the following properties:
   *        - visible: true if the pane should be shown, false to hide
   *        - animated: true to display an animation on toggle
   *        - delayed: true to wait a few cycles before toggle
   *        - callback: a function to invoke when the toggle finishes
   * @param nsIDOMNode aPane
   *        The element representing the pane to toggle.
   */
  togglePane: function(aFlags, aPane) {
    // Hiding is always handled via margins, not the hidden attribute.
    aPane.removeAttribute("hidden");

    // Add a class to the pane to handle min-widths, margins and animations.
    if (!aPane.classList.contains("generic-toggled-side-pane")) {
      aPane.classList.add("generic-toggled-side-pane");
    }

    // Avoid useless toggles.
    if (aFlags.visible == !aPane.hasAttribute("pane-collapsed")) {
      if (aFlags.callback) aFlags.callback();
      return;
    }

    // Computes and sets the pane margins in order to hide or show it.
    function set() {
      if (aFlags.visible) {
        aPane.style.marginLeft = "0";
        aPane.style.marginRight = "0";
        aPane.removeAttribute("pane-collapsed");
      } else {
        let margin = ~~(aPane.getAttribute("width")) + 1;
        aPane.style.marginLeft = -margin + "px";
        aPane.style.marginRight = -margin + "px";
        aPane.setAttribute("pane-collapsed", "");
      }

      // Invoke the callback when the transition ended.
      if (aFlags.animated) {
        aPane.addEventListener("transitionend", function onEvent() {
          aPane.removeEventListener("transitionend", onEvent, false);
          if (aFlags.callback) aFlags.callback();
        }, false);
      }
      // Invoke the callback immediately since there's no transition.
      else {
        if (aFlags.callback) aFlags.callback();
      }
    }

    // The "animated" attributes enables animated toggles (slide in-out).
    if (aFlags.animated) {
      aPane.setAttribute("animated", "");
    } else {
      aPane.removeAttribute("animated");
    }

    // Sometimes it's useful delaying the toggle a few ticks to ensure
    // a smoother slide in-out animation.
    if (aFlags.delayed) {
      aPane.ownerDocument.defaultView.setTimeout(set.bind(this), PANE_APPEARANCE_DELAY);
    } else {
      set.call(this);
    }
  }
};

/**
 * Localization convenience methods.
 *
 * @param string aStringBundleName
 *        The desired string bundle's name.
 */
ViewHelpers.L10N = function(aStringBundleName) {
  XPCOMUtils.defineLazyGetter(this, "stringBundle", () =>
    Services.strings.createBundle(aStringBundleName));

  XPCOMUtils.defineLazyGetter(this, "ellipsis", () =>
    Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data);
};

ViewHelpers.L10N.prototype = {
  stringBundle: null,

  /**
   * L10N shortcut function.
   *
   * @param string aName
   * @return string
   */
  getStr: function(aName) {
    return this.stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function.
   *
   * @param string aName
   * @param array aArgs
   * @return string
   */
  getFormatStr: function(aName, ...aArgs) {
    return this.stringBundle.formatStringFromName(aName, aArgs, aArgs.length);
  },

  /**
   * L10N shortcut function for numeric arguments that need to be formatted.
   * All numeric arguments will be fixed to 2 decimals and given a localized
   * decimal separator. Other arguments will be left alone.
   *
   * @param string aName
   * @param array aArgs
   * @return string
   */
  getFormatStrWithNumbers: function(aName, ...aArgs) {
    let newArgs = aArgs.map(x => typeof x == "number" ? this.numberWithDecimals(x, 2) : x);
    return this.stringBundle.formatStringFromName(aName, newArgs, newArgs.length);
  },

  /**
   * Converts a number to a locale-aware string format and keeps a certain
   * number of decimals.
   *
   * @param number aNumber
   *        The number to convert.
   * @param number aDecimals [optional]
   *        Total decimals to keep.
   * @return string
   *         The localized number as a string.
   */
  numberWithDecimals: function(aNumber, aDecimals = 0) {
    // If this is an integer, don't do anything special.
    if (aNumber == (aNumber | 0)) {
      return aNumber;
    }
    // Remove {n} trailing decimals. Can't use toFixed(n) because
    // toLocaleString converts the number to a string. Also can't use
    // toLocaleString(, { maximumFractionDigits: n }) because it's not
    // implemented on OS X (bug 368838). Gross.
    let localized = aNumber.toLocaleString(); // localize
    let padded = localized + new Array(aDecimals).join("0"); // pad with zeros
    let match = padded.match("([^]*?\\d{" + aDecimals + "})\\d*$");
    return match.pop();
  }
};

/**
 * Shortcuts for lazily accessing and setting various preferences.
 * Usage:
 *   let prefs = new ViewHelpers.Prefs("root.path.to.branch", {
 *     myIntPref: ["Int", "leaf.path.to.my-int-pref"],
 *     myCharPref: ["Char", "leaf.path.to.my-char-pref"],
 *     ...
 *   });
 *
 *   prefs.myCharPref = "foo";
 *   let aux = prefs.myCharPref;
 *
 * @param string aPrefsRoot
 *        The root path to the required preferences branch.
 * @param object aPrefsObject
 *        An object containing { accessorName: [prefType, prefName] } keys.
 */
ViewHelpers.Prefs = function(aPrefsRoot = "", aPrefsObject = {}) {
  this.root = aPrefsRoot;

  for (let accessorName in aPrefsObject) {
    let [prefType, prefName] = aPrefsObject[accessorName];
    this.map(accessorName, prefType, prefName);
  }
};

ViewHelpers.Prefs.prototype = {
  /**
   * Helper method for getting a pref value.
   *
   * @param string aType
   * @param string aPrefName
   * @return any
   */
  _get: function(aType, aPrefName) {
    if (this[aPrefName] === undefined) {
      this[aPrefName] = Services.prefs["get" + aType + "Pref"](aPrefName);
    }
    return this[aPrefName];
  },

  /**
   * Helper method for setting a pref value.
   *
   * @param string aType
   * @param string aPrefName
   * @param any aValue
   */
  _set: function(aType, aPrefName, aValue) {
    Services.prefs["set" + aType + "Pref"](aPrefName, aValue);
    this[aPrefName] = aValue;
  },

  /**
   * Maps a property name to a pref, defining lazy getters and setters.
   *
   * @param string aAccessorName
   * @param string aType
   * @param string aPrefName
   */
  map: function(aAccessorName, aType, aPrefName) {
    Object.defineProperty(this, aAccessorName, {
      get: () => this._get(aType, [this.root, aPrefName].join(".")),
      set: (aValue) => this._set(aType, [this.root, aPrefName].join("."), aValue)
    });
  }
};

/**
 * A generic Item is used to describe children present in a Widget.
 * The label, value and description properties are necessarily strings.
 * Iterable via "for (let childItem in parentItem) { }".
 *
 * @param any aAttachment
 *        Some attached primitive/object.
 * @param nsIDOMNode | nsIDOMDocumentFragment | array aContents [optional]
 *        A prebuilt node, or an array containing the following properties:
 *        - aLabel: the label displayed in the widget
 *        - aValue: the actual internal value of the item
 *        - aDescription: an optional description of the item
 */
function Item(aAttachment, aContents = []) {
  this.attachment = aAttachment;

  // Allow the insertion of prebuilt nodes.
  if (aContents instanceof Ci.nsIDOMNode ||
      aContents instanceof Ci.nsIDOMDocumentFragment) {
    this._prebuiltTarget = aContents;
  }
  // Delegate the item view creation to a widget.
  else {
    let [aLabel, aValue, aDescription] = aContents;
    this._label = aLabel + "";
    this._value = aValue + "";
    this._description = (aDescription || "") + "";
  }

  XPCOMUtils.defineLazyGetter(this, "_itemsByElement", () => new Map());
};

Item.prototype = {
  get label() this._label,
  get value() this._value,
  get description() this._description,
  get target() this._target,

  /**
   * Immediately appends a child item to this item.
   *
   * @param nsIDOMNode aElement
   *        An nsIDOMNode representing the child element to append.
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - attachment: some attached primitive/object for the item
   *          - attributes: a batch of attributes set to the displayed element
   *          - finalize: function invoked when the child item is removed
   * @return Item
   *         The item associated with the displayed element.
   */
  append: function(aElement, aOptions = {}) {
    let item = new Item(aOptions.attachment);

    // Entangle the item with the newly inserted child node.
    this._entangleItem(item, this._target.appendChild(aElement));

    // Handle any additional options after entangling the item.
    if (aOptions.attributes) {
      aOptions.attributes.forEach(e => item._target.setAttribute(e[0], e[1]));
    }
    if (aOptions.finalize) {
      item.finalize = aOptions.finalize;
    }

    // Return the item associated with the displayed element.
    return item;
  },

  /**
   * Immediately removes the specified child item from this item.
   *
   * @param Item aItem
   *        The item associated with the element to remove.
   */
  remove: function(aItem) {
    if (!aItem) {
      return;
    }
    this._target.removeChild(aItem._target);
    this._untangleItem(aItem);
  },

  /**
   * Entangles an item (model) with a displayed node element (view).
   *
   * @param Item aItem
   *        The item describing a target element.
   * @param nsIDOMNode aElement
   *        The element displaying the item.
   */
  _entangleItem: function(aItem, aElement) {
    this._itemsByElement.set(aElement, aItem);
    aItem._target = aElement;
  },

  /**
   * Untangles an item (model) from a displayed node element (view).
   *
   * @param Item aItem
   *        The item describing a target element.
   */
  _untangleItem: function(aItem) {
    if (aItem.finalize) {
      aItem.finalize(aItem);
    }
    for (let childItem in aItem) {
      aItem.remove(childItem);
    }

    this._unlinkItem(aItem);
    aItem._prebuiltTarget = null;
    aItem._target = null;
  },

  /**
   * Deletes an item from the its parent's storage maps.
   *
   * @param Item aItem
   *        The item describing a target element.
   */
  _unlinkItem: function(aItem) {
    this._itemsByElement.delete(aItem._target);
  },

  /**
   * Returns a string representing the object.
   * @return string
   */
  toString: function() {
    if (this._label && this._value) {
      return this._label + " -> " + this._value;
    }
    if (this.attachment) {
      return this.attachment.toString();
    }
    return "(null)";
  },

  _label: "",
  _value: "",
  _description: "",
  _prebuiltTarget: null,
  _target: null,
  finalize: null,
  attachment: null
};

/**
 * Some generic Widget methods handling Item instances.
 * Iterable via "for (let childItem in wrappedView) { }".
 *
 * Usage:
 *   function MyView() {
 *     this.widget = new MyWidget(document.querySelector(".my-node"));
 *   }
 *
 *   MyView.prototype = Heritage.extend(WidgetMethods, {
 *     myMethod: function() {},
 *     ...
 *   });
 *
 * See https://gist.github.com/victorporof/5749386 for more details.
 *
 * Language:
 *   - An "item" is an instance of an Item.
 *   - An "element" or "node" is a nsIDOMNode.
 *
 * The supplied element node or widget can either be a <xul:menulist>, or any
 * other object interfacing the following methods:
 *   - function:nsIDOMNode insertItemAt(aIndex:number, aLabel:string, aValue:string)
 *   - function:nsIDOMNode getItemAtIndex(aIndex:number)
 *   - function removeChild(aChild:nsIDOMNode)
 *   - function removeAllItems()
 *   - get:nsIDOMNode selectedItem()
 *   - set selectedItem(aChild:nsIDOMNode)
 *   - function getAttribute(aName:string)
 *   - function setAttribute(aName:string, aValue:string)
 *   - function removeAttribute(aName:string)
 *   - function addEventListener(aName:string, aCallback:function, aBubbleFlag:boolean)
 *   - function removeEventListener(aName:string, aCallback:function, aBubbleFlag:boolean)
 *
 * For automagical keyboard and mouse accessibility, the element node or widget
 * should be an event emitter with the following events:
 *   - "keyPress" -> (aName:string, aEvent:KeyboardEvent)
 *   - "mousePress" -> (aName:string, aEvent:MouseEvent)
 */
this.WidgetMethods = {
  /**
   * Sets the element node or widget associated with this container.
   * @param nsIDOMNode | object aWidget
   */
  set widget(aWidget) {
    this._widget = aWidget;

    // Can't use WeakMaps for itemsByLabel or itemsByValue because
    // keys are strings, and itemsByElement needs to be iterable.
    XPCOMUtils.defineLazyGetter(this, "_itemsByLabel", () => new Map());
    XPCOMUtils.defineLazyGetter(this, "_itemsByValue", () => new Map());
    XPCOMUtils.defineLazyGetter(this, "_itemsByElement", () => new Map());
    XPCOMUtils.defineLazyGetter(this, "_stagedItems", () => []);

    // Handle internal events emitted by the widget if necessary.
    if (ViewHelpers.isEventEmitter(aWidget)) {
      aWidget.on("keyPress", this._onWidgetKeyPress.bind(this));
      aWidget.on("mousePress", this._onWidgetMousePress.bind(this));
    }
  },

  /**
   * Gets the element node or widget associated with this container.
   * @return nsIDOMNode | object
   */
  get widget() this._widget,

  /**
   * Prepares an item to be added to this container. This allows, for example,
   * for a large number of items to be batched up before being sorted & added.
   *
   * If the "staged" flag is *not* set to true, the item will be immediately
   * inserted at the correct position in this container, so that all the items
   * still remain sorted. This can (possibly) be much slower than batching up
   * multiple items.
   *
   * By default, this container assumes that all the items should be displayed
   * sorted by their label. This can be overridden with the "index" flag,
   * specifying on which position should an item be appended. The "staged" and
   * "index" flags are mutually exclusive, meaning that all staged items
   * will always be appended.
   *
   * Furthermore, this container makes sure that all the items are unique
   * (two items with the same label or value are not allowed) and non-degenerate
   * (items with "undefined" or "null" labels/values). This can, as well, be
   * overridden via the "relaxed" flag.
   *
   * @param nsIDOMNode | nsIDOMDocumentFragment array aContents
   *        A prebuilt node, or an array containing the following properties:
   *          - label: the label displayed in the container
   *          - value: the actual internal value of the item
   *          - description: an optional description of the item
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - staged: true to stage the item to be appended later
   *          - index: specifies on which position should the item be appended
   *          - relaxed: true if this container should allow dupes & degenerates
   *          - attachment: some attached primitive/object for the item
   *          - attributes: a batch of attributes set to the displayed element
   *          - finalize: function invoked when the item is removed
   * @return Item
   *         The item associated with the displayed element if an unstaged push,
   *         undefined if the item was staged for a later commit.
   */
  push: function(aContents, aOptions = {}) {
    let item = new Item(aOptions.attachment, aContents);

    // Batch the item to be added later.
    if (aOptions.staged) {
      // An ulterior commit operation will ignore any specified index.
      delete aOptions.index;
      return void this._stagedItems.push({ item: item, options: aOptions });
    }
    // Find the target position in this container and insert the item there.
    if (!("index" in aOptions)) {
      return this._insertItemAt(this._findExpectedIndexFor(item), item, aOptions);
    }
    // Insert the item at the specified index. If negative or out of bounds,
    // the item will be simply appended.
    return this._insertItemAt(aOptions.index, item, aOptions);
  },

  /**
   * Flushes all the prepared items into this container.
   * Any specified index on the items will be ignored. Everything is appended.
   *
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - sorted: true to sort all the items before adding them
   */
  commit: function(aOptions = {}) {
    let stagedItems = this._stagedItems;

    // Sort the items before adding them to this container, if preferred.
    if (aOptions.sorted) {
      stagedItems.sort((a, b) => this._currentSortPredicate(a.item, b.item));
    }
    // Append the prepared items to this container.
    for (let { item, options } of stagedItems) {
      this._insertItemAt(-1, item, options);
    }
    // Recreate the temporary items list for ulterior pushes.
    this._stagedItems.length = 0;
  },

  /**
   * Updates this container to reflect the information provided by the
   * currently selected item.
   *
   * @return boolean
   *         True if a selected item was available, false otherwise.
   */
  refresh: function() {
    let selectedItem = this.selectedItem;
    if (!selectedItem) {
      return false;
    }
    this._widget.removeAttribute("notice");
    this._widget.setAttribute("label", selectedItem._label);
    this._widget.setAttribute("tooltiptext", selectedItem._value);
    return true;
  },

  /**
   * Immediately removes the specified item from this container.
   *
   * @param Item aItem
   *        The item associated with the element to remove.
   */
  remove: function(aItem) {
    if (!aItem) {
      return;
    }
    this._widget.removeChild(aItem._target);
    this._untangleItem(aItem);
  },

  /**
   * Removes the item at the specified index from this container.
   *
   * @param number aIndex
   *        The index of the item to remove.
   */
  removeAt: function(aIndex) {
    this.remove(this.getItemAtIndex(aIndex));
  },

  /**
   * Removes all items from this container.
   */
  empty: function() {
    this._preferredValue = this.selectedValue;
    this._widget.selectedItem = null;
    this._widget.removeAllItems();
    this._widget.setAttribute("notice", this.emptyText);
    this._widget.setAttribute("label", this.emptyText);
    this._widget.removeAttribute("tooltiptext");

    for (let [, item] of this._itemsByElement) {
      this._untangleItem(item);
    }

    this._itemsByLabel.clear();
    this._itemsByValue.clear();
    this._itemsByElement.clear();
    this._stagedItems.length = 0;
  },

  /**
   * Does not remove any item in this container. Instead, it overrides the
   * current label to signal that it is unavailable and removes the tooltip.
   */
  setUnavailable: function() {
    this._widget.setAttribute("notice", this.unavailableText);
    this._widget.setAttribute("label", this.unavailableText);
    this._widget.removeAttribute("tooltiptext");
  },

  /**
   * The label string automatically added to this container when there are
   * no child nodes present.
   */
  emptyText: "",

  /**
   * The label string added to this container when it is marked as unavailable.
   */
  unavailableText: "",

  /**
   * Toggles all the items in this container hidden or visible.
   *
   * This does not change the default filtering predicate, so newly inserted
   * items will always be visible. Use WidgetMethods.filterContents if you care.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  toggleContents: function(aVisibleFlag) {
    for (let [element, item] of this._itemsByElement) {
      element.hidden = !aVisibleFlag;
    }
  },

  /**
   * Toggles all items in this container hidden or visible based on a predicate.
   *
   * @param function aPredicate [optional]
   *        Items are toggled according to the return value of this function,
   *        which will become the new default filtering predicate in this container.
   *        If unspecified, all items will be toggled visible.
   */
  filterContents: function(aPredicate = this._currentFilterPredicate) {
    this._currentFilterPredicate = aPredicate;

    for (let [element, item] of this._itemsByElement) {
      element.hidden = !aPredicate(item);
    }
  },

  /**
   * Sorts all the items in this container based on a predicate.
   *
   * @param function aPredicate [optional]
   *        Items are sorted according to the return value of the function,
   *        which will become the new default sorting predicate in this container.
   *        If unspecified, all items will be sorted by their label.
   */
  sortContents: function(aPredicate = this._currentSortPredicate) {
    let sortedItems = this.orderedItems.sort(this._currentSortPredicate = aPredicate);

    for (let i = 0, len = sortedItems.length; i < len; i++) {
      this.swapItems(this.getItemAtIndex(i), sortedItems[i]);
    }
  },

  /**
   * Visually swaps two items in this container.
   *
   * @param Item aFirst
   *        The first item to be swapped.
   * @param Item aSecond
   *        The second item to be swapped.
   */
  swapItems: function(aFirst, aSecond) {
    if (aFirst == aSecond) { // We're just dandy, thank you.
      return;
    }
    let { _prebuiltTarget: firstPrebuiltTarget, target: firstTarget } = aFirst;
    let { _prebuiltTarget: secondPrebuiltTarget, target: secondTarget } = aSecond;

    // If the two items were constructed with prebuilt nodes as DocumentFragments,
    // then those DocumentFragments are now empty and need to be reassembled.
    if (firstPrebuiltTarget instanceof Ci.nsIDOMDocumentFragment) {
      for (let node of firstTarget.childNodes) {
        firstPrebuiltTarget.appendChild(node.cloneNode(true));
      }
    }
    if (secondPrebuiltTarget instanceof Ci.nsIDOMDocumentFragment) {
      for (let node of secondTarget.childNodes) {
        secondPrebuiltTarget.appendChild(node.cloneNode(true));
      }
    }

    // 1. Get the indices of the two items to swap.
    let i = this._indexOfElement(firstTarget);
    let j = this._indexOfElement(secondTarget);

    // 2. Remeber the selection index, to reselect an item, if necessary.
    let selectedTarget = this._widget.selectedItem;
    let selectedIndex = -1;
    if (selectedTarget == firstTarget) {
      selectedIndex = i;
    } else if (selectedTarget == secondTarget) {
      selectedIndex = j;
    }

    // 3. Silently nuke both items, nobody needs to know about this.
    this._widget.removeChild(firstTarget);
    this._widget.removeChild(secondTarget);
    this._unlinkItem(aFirst);
    this._unlinkItem(aSecond);

    // 4. Add the items again, but reversing their indices.
    this._insertItemAt.apply(this, i < j ? [i, aSecond] : [j, aFirst]);
    this._insertItemAt.apply(this, i < j ? [j, aFirst] : [i, aSecond]);

    // 5. Restore the previous selection, if necessary.
    if (selectedIndex == i) {
      this._widget.selectedItem = aFirst._target;
    } else if (selectedIndex == j) {
      this._widget.selectedItem = aSecond._target;
    }
  },

  /**
   * Visually swaps two items in this container at specific indices.
   *
   * @param number aFirst
   *        The index of the first item to be swapped.
   * @param number aSecond
   *        The index of the second item to be swapped.
   */
  swapItemsAtIndices: function(aFirst, aSecond) {
    this.swapItems(this.getItemAtIndex(aFirst), this.getItemAtIndex(aSecond));
  },

  /**
   * Checks whether an item with the specified label is among the elements
   * shown in this container.
   *
   * @param string aLabel
   *        The item's label.
   * @return boolean
   *         True if the label is known, false otherwise.
   */
  containsLabel: function(aLabel) {
    return this._itemsByLabel.has(aLabel) ||
           this._stagedItems.some(({ item }) => item._label == aLabel);
  },

  /**
   * Checks whether an item with the specified value is among the elements
   * shown in this container.
   *
   * @param string aValue
   *        The item's value.
   * @return boolean
   *         True if the value is known, false otherwise.
   */
  containsValue: function(aValue) {
    return this._itemsByValue.has(aValue) ||
           this._stagedItems.some(({ item }) => item._value == aValue);
  },

  /**
   * Gets the "preferred value". This is the latest selected item's value,
   * remembered just before emptying this container.
   * @return string
   */
  get preferredValue() this._preferredValue,

  /**
   * Retrieves the item associated with the selected element.
   * @return Item
   */
  get selectedItem() {
    let selectedElement = this._widget.selectedItem;
    if (selectedElement) {
      return this._itemsByElement.get(selectedElement);
    }
    return null;
  },

  /**
   * Retrieves the selected element's index in this container.
   * @return number
   */
  get selectedIndex() {
    let selectedElement = this._widget.selectedItem;
    if (selectedElement) {
      return this._indexOfElement(selectedElement);
    }
    return -1;
  },

  /**
   * Retrieves the label of the selected element.
   * @return string
   */
  get selectedLabel() {
    let selectedElement = this._widget.selectedItem;
    if (selectedElement) {
      return this._itemsByElement.get(selectedElement)._label;
    }
    return "";
  },

  /**
   * Retrieves the value of the selected element.
   * @return string
   */
  get selectedValue() {
    let selectedElement = this._widget.selectedItem;
    if (selectedElement) {
      return this._itemsByElement.get(selectedElement)._value;
    }
    return "";
  },

  /**
   * Selects the element with the entangled item in this container.
   * @param Item | function aItem
   */
  set selectedItem(aItem) {
    // A predicate is allowed to select a specific item.
    // If no item is matched, then the current selection is removed.
    if (typeof aItem == "function") {
      aItem = this.getItemForPredicate(aItem);
    }

    // A falsy item is allowed to invalidate the current selection.
    let targetElement = aItem ? aItem._target : null;
    let prevElement = this._widget.selectedItem;

    // Make sure the currently selected item's target element is also focused.
    if (this.autoFocusOnSelection && targetElement) {
      targetElement.focus();
    }

    // Prevent selecting the same item again and avoid dispatching
    // a redundant selection event, so return early.
    if (targetElement == prevElement) {
      return;
    }
    this._widget.selectedItem = targetElement;
    ViewHelpers.dispatchEvent(targetElement || prevElement, "select", aItem);

    // Updates this container to reflect the information provided by the
    // currently selected item.
    this.refresh();
  },

  /**
   * Selects the element at the specified index in this container.
   * @param number aIndex
   */
  set selectedIndex(aIndex) {
    let targetElement = this._widget.getItemAtIndex(aIndex);
    if (targetElement) {
      this.selectedItem = this._itemsByElement.get(targetElement);
      return;
    }
    this.selectedItem = null;
  },

  /**
   * Selects the element with the specified label in this container.
   * @param string aLabel
   */
  set selectedLabel(aLabel)
    this.selectedItem = this._itemsByLabel.get(aLabel),

  /**
   * Selects the element with the specified value in this container.
   * @param string aValue
   */
  set selectedValue(aValue)
    this.selectedItem = this._itemsByValue.get(aValue),

  /**
   * Focus this container the first time an element is inserted?
   *
   * If this flag is set to true, then when the first item is inserted in
   * this container (and thus it's the only item available), its corresponding
   * target element is focused as well.
   */
  autoFocusOnFirstItem: true,

  /**
   * Focus on selection?
   *
   * If this flag is set to true, then whenever an item is selected in
   * this container (e.g. via the selectedIndex or selectedItem setters),
   * its corresponding target element is focused as well.
   *
   * You can disable this flag, for example, to maintain a certain node
   * focused but visually indicate a different selection in this container.
   */
  autoFocusOnSelection: true,

  /**
   * Focus on input (e.g. mouse click)?
   *
   * If this flag is set to true, then whenever an item receives user input in
   * this container, its corresponding target element is focused as well.
   */
  autoFocusOnInput: true,

  /**
   * The number of elements in this container to jump when Page Up or Page Down
   * keys are pressed. If falsy, then the page size will be based on the
   * number of visible items in the container.
   */
  pageSize: 0,

  /**
   * Focuses the first visible item in this container.
   */
  focusFirstVisibleItem: function() {
    this.focusItemAtDelta(-this.itemCount);
  },

  /**
   * Focuses the last visible item in this container.
   */
  focusLastVisibleItem: function() {
    this.focusItemAtDelta(+this.itemCount);
  },

  /**
   * Focuses the next item in this container.
   */
  focusNextItem: function() {
    this.focusItemAtDelta(+1);
  },

  /**
   * Focuses the previous item in this container.
   */
  focusPrevItem: function() {
    this.focusItemAtDelta(-1);
  },

  /**
   * Focuses another item in this container based on the index distance
   * from the currently focused item.
   *
   * @param number aDelta
   *        A scalar specifying by how many items should the selection change.
   */
  focusItemAtDelta: function(aDelta) {
    // Make sure the currently selected item is also focused, so that the
    // command dispatcher mechanism has a relative node to work with.
    // If there's no selection, just select an item at a corresponding index
    // (e.g. the first item in this container if aDelta <= 1).
    let selectedElement = this._widget.selectedItem;
    if (selectedElement) {
      selectedElement.focus();
    } else {
      this.selectedIndex = Math.max(0, aDelta - 1);
      return;
    }

    let direction = aDelta > 0 ? "advanceFocus" : "rewindFocus";
    let distance = Math.abs(Math[aDelta > 0 ? "ceil" : "floor"](aDelta));
    while (distance--) {
      if (!this._focusChange(direction)) {
        break; // Out of bounds.
      }
    }

    // Synchronize the selected item as being the currently focused element.
    this.selectedItem = this.getItemForElement(this._focusedElement);
  },

  /**
   * Focuses the next or previous item in this container.
   *
   * @param string aDirection
   *        Either "advanceFocus" or "rewindFocus".
   * @return boolean
   *         False if the focus went out of bounds and the first or last item
   *         in this container was focused instead.
   */
  _focusChange: function(aDirection) {
    let commandDispatcher = this._commandDispatcher;
    let prevFocusedElement = commandDispatcher.focusedElement;

    commandDispatcher.suppressFocusScroll = true;
    commandDispatcher[aDirection]();

    // Make sure the newly focused item is a part of this container.
    // If the focus goes out of bounds, revert the previously focused item.
    if (!this.getItemForElement(commandDispatcher.focusedElement)) {
      prevFocusedElement.focus();
      return false;
    }
    // Focus remained within bounds.
    return true;
  },

  /**
   * Gets the command dispatcher instance associated with this container's DOM.
   * If there are no items displayed in this container, null is returned.
   * @return nsIDOMXULCommandDispatcher | null
   */
  get _commandDispatcher() {
    if (this._cachedCommandDispatcher) {
      return this._cachedCommandDispatcher;
    }
    let someElement = this._widget.getItemAtIndex(0);
    if (someElement) {
      let commandDispatcher = someElement.ownerDocument.commandDispatcher;
      return this._cachedCommandDispatcher = commandDispatcher;
    }
    return null;
  },

  /**
   * Gets the currently focused element in this container.
   *
   * @return nsIDOMNode
   *         The focused element, or null if nothing is found.
   */
  get _focusedElement() {
    let commandDispatcher = this._commandDispatcher;
    if (commandDispatcher) {
      return commandDispatcher.focusedElement;
    }
    return null;
  },

  /**
   * Gets the item in the container having the specified index.
   *
   * @param number aIndex
   *        The index used to identify the element.
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemAtIndex: function(aIndex) {
    return this.getItemForElement(this._widget.getItemAtIndex(aIndex));
  },

  /**
   * Gets the item in the container having the specified label.
   *
   * @param string aLabel
   *        The label used to identify the element.
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemByLabel: function(aLabel) {
    return this._itemsByLabel.get(aLabel);
  },

  /**
   * Gets the item in the container having the specified value.
   *
   * @param string aValue
   *        The value used to identify the element.
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemByValue: function(aValue) {
    return this._itemsByValue.get(aValue);
  },

  /**
   * Gets the item in the container associated with the specified element.
   *
   * @param nsIDOMNode aElement
   *        The element used to identify the item.
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemForElement: function(aElement) {
    while (aElement) {
      let item = this._itemsByElement.get(aElement);
      if (item) {
        return item;
      }
      aElement = aElement.parentNode;
    }
    return null;
  },

  /**
   * Gets a visible item in this container validating a specified predicate.
   *
   * @param function aPredicate
   *        The first item which validates this predicate is returned
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemForPredicate: function(aPredicate, aOwner = this) {
    for (let [element, item] of aOwner._itemsByElement) {
      let match;
      if (aPredicate(item) && !element.hidden) {
        match = item;
      } else {
        match = this.getItemForPredicate(aPredicate, item);
      }
      if (match) {
        return match;
      }
    }
    return null;
  },

  /**
   * Finds the index of an item in the container.
   *
   * @param Item aItem
   *        The item get the index for.
   * @return number
   *         The index of the matched item, or -1 if nothing is found.
   */
  indexOfItem: function(aItem) {
    return this._indexOfElement(aItem._target);
  },

  /**
   * Finds the index of an element in the container.
   *
   * @param nsIDOMNode aElement
   *        The element get the index for.
   * @return number
   *         The index of the matched element, or -1 if nothing is found.
   */
  _indexOfElement: function(aElement) {
    for (let i = 0; i < this._itemsByElement.size; i++) {
      if (this._widget.getItemAtIndex(i) == aElement) {
        return i;
      }
    }
    return -1;
  },

  /**
   * Gets the total number of items in this container.
   * @return number
   */
  get itemCount() this._itemsByElement.size,

  /**
   * Returns a list of items in this container, in no particular order.
   * @return array
   */
  get items() {
    let items = [];
    for (let [, item] of this._itemsByElement) {
      items.push(item);
    }
    return items;
  },

  /**
   * Returns a list of labels in this container, in no particular order.
   * @return array
   */
  get labels() {
    let labels = [];
    for (let [label] of this._itemsByLabel) {
      labels.push(label);
    }
    return labels;
  },

  /**
   * Returns a list of values in this container, in no particular order.
   * @return array
   */
  get values() {
    let values = [];
    for (let [value] of this._itemsByValue) {
      values.push(value);
    }
    return values;
  },

  /**
   * Returns a list of all the visible (non-hidden) items in this container,
   * in no particular order.
   * @return array
   */
  get visibleItems() {
    let items = [];
    for (let [element, item] of this._itemsByElement) {
      if (!element.hidden) {
        items.push(item);
      }
    }
    return items;
  },

  /**
   * Returns a list of all items in this container, in the displayed order.
   * @return array
   */
  get orderedItems() {
    let items = [];
    let itemCount = this.itemCount;
    for (let i = 0; i < itemCount; i++) {
      items.push(this.getItemAtIndex(i));
    }
    return items;
  },

  /**
   * Returns a list of all the visible (non-hidden) items in this container,
   * in the displayed order
   * @return array
   */
  get orderedVisibleItems() {
    let items = [];
    let itemCount = this.itemCount;
    for (let i = 0; i < itemCount; i++) {
      let item = this.getItemAtIndex(i);
      if (!item._target.hidden) {
        items.push(item);
      }
    }
    return items;
  },

  /**
   * Specifies the required conditions for an item to be considered unique.
   * Possible values:
   *   - 1: label AND value are different from all other items
   *   - 2: label OR value are different from all other items
   *   - 3: only label is required to be different
   *   - 4: only value is required to be different
   */
  uniquenessQualifier: 1,

  /**
   * Checks if an item is unique in this container.
   *
   * @param Item aItem
   *        The item for which to verify uniqueness.
   * @return boolean
   *         True if the item is unique, false otherwise.
   */
  isUnique: function(aItem) {
    switch (this.uniquenessQualifier) {
      case 1:
        return !this._itemsByLabel.has(aItem._label) &&
               !this._itemsByValue.has(aItem._value);
      case 2:
        return !this._itemsByLabel.has(aItem._label) ||
               !this._itemsByValue.has(aItem._value);
      case 3:
        return !this._itemsByLabel.has(aItem._label);
      case 4:
        return !this._itemsByValue.has(aItem._value);
    }
    return false;
  },

  /**
   * Checks if an item is eligible for this container.
   *
   * @param Item aItem
   *        The item for which to verify eligibility.
   * @return boolean
   *         True if the item is eligible, false otherwise.
   */
  isEligible: function(aItem) {
    let isUnique = this.isUnique(aItem);
    let isPrebuilt = !!aItem._prebuiltTarget;
    let isDegenerate = aItem._label == "undefined" || aItem._label == "null" ||
                       aItem._value == "undefined" || aItem._value == "null";

    return isPrebuilt || (isUnique && !isDegenerate);
  },

  /**
   * Finds the expected item index in this container based on the default
   * sort predicate.
   *
   * @param Item aItem
   *        The item for which to get the expected index.
   * @return number
   *         The expected item index.
   */
  _findExpectedIndexFor: function(aItem) {
    let itemCount = this.itemCount;

    for (let i = 0; i < itemCount; i++) {
      if (this._currentSortPredicate(this.getItemAtIndex(i), aItem) > 0) {
        return i;
      }
    }
    return itemCount;
  },

  /**
   * Immediately inserts an item in this container at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param Item aItem
   *        An object containing a label and a value property (at least).
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - node: allows the insertion of prebuilt nodes instead of labels
   *          - relaxed: true if this container should allow dupes & degenerates
   *          - attributes: a batch of attributes set to the displayed element
   *          - finalize: function when the item is untangled (removed)
   * @return Item
   *         The item associated with the displayed element, null if rejected.
   */
  _insertItemAt: function(aIndex, aItem, aOptions = {}) {
    // Relaxed nodes may be appended without verifying their eligibility.
    if (!aOptions.relaxed && !this.isEligible(aItem)) {
      return null;
    }

    // Entangle the item with the newly inserted node.
    this._entangleItem(aItem, this._widget.insertItemAt(aIndex,
      aItem._prebuiltTarget || aItem._label, // Allow the insertion of prebuilt nodes.
      aItem._value,
      aItem._description,
      aItem.attachment));

    // Handle any additional options after entangling the item.
    if (!this._currentFilterPredicate(aItem)) {
      aItem._target.hidden = true;
    }
    if (this.autoFocusOnFirstItem && this._itemsByElement.size == 1) {
      aItem._target.focus();
    }
    if (aOptions.attributes) {
      aOptions.attributes.forEach(e => aItem._target.setAttribute(e[0], e[1]));
    }
    if (aOptions.finalize) {
      aItem.finalize = aOptions.finalize;
    }

    // Return the item associated with the displayed element.
    return aItem;
  },

  /**
   * Entangles an item (model) with a displayed node element (view).
   *
   * @param Item aItem
   *        The item describing a target element.
   * @param nsIDOMNode aElement
   *        The element displaying the item.
   */
  _entangleItem: function(aItem, aElement) {
    this._itemsByLabel.set(aItem._label, aItem);
    this._itemsByValue.set(aItem._value, aItem);
    this._itemsByElement.set(aElement, aItem);
    aItem._target = aElement;
  },

  /**
   * Untangles an item (model) from a displayed node element (view).
   *
   * @param Item aItem
   *        The item describing a target element.
   */
  _untangleItem: function(aItem) {
    if (aItem.finalize) {
      aItem.finalize(aItem);
    }
    for (let childItem in aItem) {
      aItem.remove(childItem);
    }

    this._unlinkItem(aItem);
    aItem._prebuiltTarget = null;
    aItem._target = null;
  },

  /**
   * Deletes an item from the its parent's storage maps.
   *
   * @param Item aItem
   *        The item describing a target element.
   */
  _unlinkItem: function(aItem) {
    this._itemsByLabel.delete(aItem._label);
    this._itemsByValue.delete(aItem._value);
    this._itemsByElement.delete(aItem._target);
  },

  /**
   * The keyPress event listener for this container.
   * @param string aName
   * @param KeyboardEvent aEvent
   */
  _onWidgetKeyPress: function(aName, aEvent) {
    // Prevent scrolling when pressing navigation keys.
    ViewHelpers.preventScrolling(aEvent);

    switch (aEvent.keyCode) {
      case aEvent.DOM_VK_UP:
      case aEvent.DOM_VK_LEFT:
        this.focusPrevItem();
        return;
      case aEvent.DOM_VK_DOWN:
      case aEvent.DOM_VK_RIGHT:
        this.focusNextItem();
        return;
      case aEvent.DOM_VK_PAGE_UP:
        this.focusItemAtDelta(-(this.pageSize || (this.itemCount / PAGE_SIZE_ITEM_COUNT_RATIO)));
        return;
      case aEvent.DOM_VK_PAGE_DOWN:
        this.focusItemAtDelta(+(this.pageSize || (this.itemCount / PAGE_SIZE_ITEM_COUNT_RATIO)));
        return;
      case aEvent.DOM_VK_HOME:
        this.focusFirstVisibleItem();
        return;
      case aEvent.DOM_VK_END:
        this.focusLastVisibleItem();
        return;
    }
  },

  /**
   * The keyPress event listener for this container.
   * @param string aName
   * @param MouseEvent aEvent
   */
  _onWidgetMousePress: function(aName, aEvent) {
    if (aEvent.button != 0) {
      // Only allow left-click to trigger this event.
      return;
    }

    let item = this.getItemForElement(aEvent.target);
    if (item) {
      // The container is not empty and we clicked on an actual item.
      this.selectedItem = item;
      // Make sure the current event's target element is also focused.
      this.autoFocusOnInput && item._target.focus();
    }
  },

  /**
   * The predicate used when filtering items. By default, all items in this
   * view are visible.
   *
   * @param Item aItem
   *        The item passing through the filter.
   * @return boolean
   *         True if the item should be visible, false otherwise.
   */
  _currentFilterPredicate: function(aItem) {
    return true;
  },

  /**
   * The predicate used when sorting items. By default, items in this view
   * are sorted by their label.
   *
   * @param Item aFirst
   *        The first item used in the comparison.
   * @param Item aSecond
   *        The second item used in the comparison.
   * @return number
   *         -1 to sort aFirst to a lower index than aSecond
   *          0 to leave aFirst and aSecond unchanged with respect to each other
   *          1 to sort aSecond to a lower index than aFirst
   */
  _currentSortPredicate: function(aFirst, aSecond) {
    return +(aFirst._label.toLowerCase() > aSecond._label.toLowerCase());
  },

  _widget: null,
  _preferredValue: null,
  _cachedCommandDispatcher: null
};

/**
 * A generator-iterator over all the items in this container.
 */
Item.prototype.__iterator__ =
WidgetMethods.__iterator__ = function() {
  for (let [, item] of this._itemsByElement) {
    yield item;
  }
};
