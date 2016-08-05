/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");

const PANE_APPEARANCE_DELAY = 50;
const PAGE_SIZE_ITEM_COUNT_RATIO = 5;
const WIDGET_FOCUSABLE_NODES = new Set(["vbox", "hbox"]);

/**
 * Inheritance helpers from the addon SDK's core/heritage.
 * Remove these when all devtools are loadered.
 */
exports.Heritage = {
  /**
   * @see extend in sdk/core/heritage.
   */
  extend: function (prototype, properties = {}) {
    return Object.create(prototype, this.getOwnPropertyDescriptors(properties));
  },

  /**
   * @see getOwnPropertyDescriptors in sdk/core/heritage.
   */
  getOwnPropertyDescriptors: function (object) {
    return Object.getOwnPropertyNames(object).reduce((descriptor, name) => {
      descriptor[name] = Object.getOwnPropertyDescriptor(object, name);
      return descriptor;
    }, {});
  }
};

/**
 * Helper for draining a rapid succession of events and invoking a callback
 * once everything settles down.
 *
 * @param string id
 *        A string identifier for the named timeout.
 * @param number wait
 *        The amount of milliseconds to wait after no more events are fired.
 * @param function callback
 *        Invoked when no more events are fired after the specified time.
 */
const setNamedTimeout = function setNamedTimeout(id, wait, callback) {
  clearNamedTimeout(id);

  namedTimeoutsStore.set(id, setTimeout(() =>
    namedTimeoutsStore.delete(id) && callback(), wait));
};
exports.setNamedTimeout = setNamedTimeout;

/**
 * Clears a named timeout.
 * @see setNamedTimeout
 *
 * @param string id
 *        A string identifier for the named timeout.
 */
const clearNamedTimeout = function clearNamedTimeout(id) {
  if (!namedTimeoutsStore) {
    return;
  }
  clearTimeout(namedTimeoutsStore.get(id));
  namedTimeoutsStore.delete(id);
};
exports.clearNamedTimeout = clearNamedTimeout;

/**
 * Same as `setNamedTimeout`, but invokes the callback only if the provided
 * predicate function returns true. Otherwise, the timeout is re-triggered.
 *
 * @param string id
 *        A string identifier for the conditional timeout.
 * @param number wait
 *        The amount of milliseconds to wait after no more events are fired.
 * @param function predicate
 *        The predicate function used to determine whether the timeout restarts.
 * @param function callback
 *        Invoked when no more events are fired after the specified time, and
 *        the provided predicate function returns true.
 */
const setConditionalTimeout = function setConditionalTimeout(id, wait,
                                                             predicate,
                                                             callback) {
  setNamedTimeout(id, wait, function maybeCallback() {
    if (predicate()) {
      callback();
      return;
    }
    setConditionalTimeout(id, wait, predicate, callback);
  });
};
exports.setConditionalTimeout = setConditionalTimeout;

/**
 * Clears a conditional timeout.
 * @see setConditionalTimeout
 *
 * @param string id
 *        A string identifier for the conditional timeout.
 */
const clearConditionalTimeout = function clearConditionalTimeout(id) {
  clearNamedTimeout(id);
};
exports.clearConditionalTimeout = clearConditionalTimeout;

loader.lazyGetter(this, "namedTimeoutsStore", () => new Map());

/**
 * Helpers for creating and messaging between UI components.
 */
const ViewHelpers = exports.ViewHelpers = {
  /**
   * Convenience method, dispatching a custom event.
   *
   * @param nsIDOMNode target
   *        A custom target element to dispatch the event from.
   * @param string type
   *        The name of the event.
   * @param any detail
   *        The data passed when initializing the event.
   * @return boolean
   *         True if the event was cancelled or a registered handler
   *         called preventDefault.
   */
  dispatchEvent: function (target, type, detail) {
    if (!(target instanceof Ci.nsIDOMNode)) {
      // Event cancelled.
      return true;
    }
    let document = target.ownerDocument || target;
    let dispatcher = target.ownerDocument ? target : document.documentElement;

    let event = document.createEvent("CustomEvent");
    event.initCustomEvent(type, true, true, detail);
    return dispatcher.dispatchEvent(event);
  },

  /**
   * Helper delegating some of the DOM attribute methods of a node to a widget.
   *
   * @param object widget
   *        The widget to assign the methods to.
   * @param nsIDOMNode node
   *        A node to delegate the methods to.
   */
  delegateWidgetAttributeMethods: function (widget, node) {
    widget.getAttribute =
      widget.getAttribute || node.getAttribute.bind(node);
    widget.setAttribute =
      widget.setAttribute || node.setAttribute.bind(node);
    widget.removeAttribute =
      widget.removeAttribute || node.removeAttribute.bind(node);
  },

  /**
   * Helper delegating some of the DOM event methods of a node to a widget.
   *
   * @param object widget
   *        The widget to assign the methods to.
   * @param nsIDOMNode node
   *        A node to delegate the methods to.
   */
  delegateWidgetEventMethods: function (widget, node) {
    widget.addEventListener =
      widget.addEventListener || node.addEventListener.bind(node);
    widget.removeEventListener =
      widget.removeEventListener || node.removeEventListener.bind(node);
  },

  /**
   * Checks if the specified object looks like it's been decorated by an
   * event emitter.
   *
   * @return boolean
   *         True if it looks, walks and quacks like an event emitter.
   */
  isEventEmitter: function (object) {
    return object && object.on && object.off && object.once && object.emit;
  },

  /**
   * Checks if the specified object is an instance of a DOM node.
   *
   * @return boolean
   *         True if it's a node, false otherwise.
   */
  isNode: function (object) {
    return object instanceof Ci.nsIDOMNode ||
           object instanceof Ci.nsIDOMElement ||
           object instanceof Ci.nsIDOMDocumentFragment;
  },

  /**
   * Prevents event propagation when navigation keys are pressed.
   *
   * @param Event e
   *        The event to be prevented.
   */
  preventScrolling: function (e) {
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
   * Check if the enter key or space was pressed
   *
   * @param event event
   *        The event triggered by a keypress on an element
   */
  isSpaceOrReturn: function (event) {
    return event.keyCode === event.DOM_VK_SPACE ||
          event.keyCode === event.DOM_VK_RETURN;
  },

  /**
   * Sets a toggled pane hidden or visible. The pane can either be displayed on
   * the side (right or left depending on the locale) or at the bottom.
   *
   * @param object flags
   *        An object containing some of the following properties:
   *        - visible: true if the pane should be shown, false to hide
   *        - animated: true to display an animation on toggle
   *        - delayed: true to wait a few cycles before toggle
   *        - callback: a function to invoke when the toggle finishes
   * @param nsIDOMNode pane
   *        The element representing the pane to toggle.
   */
  togglePane: function (flags, pane) {
    // Make sure a pane is actually available first.
    if (!pane) {
      return;
    }

    // Hiding is always handled via margins, not the hidden attribute.
    pane.removeAttribute("hidden");

    // Add a class to the pane to handle min-widths, margins and animations.
    pane.classList.add("generic-toggled-pane");

    // Avoid useless toggles.
    if (flags.visible == !pane.classList.contains("pane-collapsed")) {
      if (flags.callback) {
        flags.callback();
      }
      return;
    }

    // The "animated" attributes enables animated toggles (slide in-out).
    if (flags.animated) {
      pane.setAttribute("animated", "");
    } else {
      pane.removeAttribute("animated");
    }

    // Computes and sets the pane margins in order to hide or show it.
    let doToggle = () => {
      // Negative margins are applied to "right" and "left" to support RTL and
      // LTR directions, as well as to "bottom" to support vertical layouts.
      // Unnecessary negative margins are forced to 0 via CSS in widgets.css.
      if (flags.visible) {
        pane.style.marginLeft = "0";
        pane.style.marginRight = "0";
        pane.style.marginBottom = "0";
        pane.classList.remove("pane-collapsed");
      } else {
        let width = Math.floor(pane.getAttribute("width")) + 1;
        let height = Math.floor(pane.getAttribute("height")) + 1;
        pane.style.marginLeft = -width + "px";
        pane.style.marginRight = -width + "px";
        pane.style.marginBottom = -height + "px";
        pane.classList.add("pane-collapsed");
      }

      // Wait for the animation to end before calling afterToggle()
      if (flags.animated) {
        pane.addEventListener("transitionend", function onEvent() {
          pane.removeEventListener("transitionend", onEvent, false);
          // Prevent unwanted transitions: if the panel is hidden and the layout
          // changes margins will be updated and the panel will pop out.
          pane.removeAttribute("animated");
          if (flags.callback) {
            flags.callback();
          }
        }, false);
      } else if (flags.callback) {
        // Invoke the callback immediately since there's no transition.
        flags.callback();
      }
    };

    // Sometimes it's useful delaying the toggle a few ticks to ensure
    // a smoother slide in-out animation.
    if (flags.delayed) {
      pane.ownerDocument.defaultView.setTimeout(doToggle,
                                                PANE_APPEARANCE_DELAY);
    } else {
      doToggle();
    }
  }
};

/**
 * A generic Item is used to describe children present in a Widget.
 *
 * This is basically a very thin wrapper around an nsIDOMNode, with a few
 * characteristics, like a `value` and an `attachment`.
 *
 * The characteristics are optional, and their meaning is entirely up to you.
 * - The `value` should be a string, passed as an argument.
 * - The `attachment` is any kind of primitive or object, passed as an argument.
 *
 * Iterable via "for (let childItem of parentItem) { }".
 *
 * @param object ownerView
 *        The owner view creating this item.
 * @param nsIDOMNode element
 *        A prebuilt node to be wrapped.
 * @param string value
 *        A string identifying the node.
 * @param any attachment
 *        Some attached primitive/object.
 */
function Item(ownerView, element, value, attachment) {
  this.ownerView = ownerView;
  this.attachment = attachment;
  this._value = value + "";
  this._prebuiltNode = element;
  this._itemsByElement = new Map();
}

Item.prototype = {
  get value() {
    return this._value;
  },
  get target() {
    return this._target;
  },
  get prebuiltNode() {
    return this._prebuiltNode;
  },

  /**
   * Immediately appends a child item to this item.
   *
   * @param nsIDOMNode element
   *        An nsIDOMNode representing the child element to append.
   * @param object options [optional]
   *        Additional options or flags supported by this operation:
   *          - attachment: some attached primitive/object for the item
   *          - attributes: a batch of attributes set to the displayed element
   *          - finalize: function invoked when the child item is removed
   * @return Item
   *         The item associated with the displayed element.
   */
  append: function (element, options = {}) {
    let item = new Item(this, element, "", options.attachment);

    // Entangle the item with the newly inserted child node.
    // Make sure this is done with the value returned by appendChild(),
    // to avoid storing a potential DocumentFragment.
    this._entangleItem(item, this._target.appendChild(element));

    // Handle any additional options after entangling the item.
    if (options.attributes) {
      options.attributes.forEach(e => item._target.setAttribute(e[0], e[1]));
    }
    if (options.finalize) {
      item.finalize = options.finalize;
    }

    // Return the item associated with the displayed element.
    return item;
  },

  /**
   * Immediately removes the specified child item from this item.
   *
   * @param Item item
   *        The item associated with the element to remove.
   */
  remove: function (item) {
    if (!item) {
      return;
    }
    this._target.removeChild(item._target);
    this._untangleItem(item);
  },

  /**
   * Entangles an item (model) with a displayed node element (view).
   *
   * @param Item item
   *        The item describing a target element.
   * @param nsIDOMNode element
   *        The element displaying the item.
   */
  _entangleItem: function (item, element) {
    this._itemsByElement.set(element, item);
    item._target = element;
  },

  /**
   * Untangles an item (model) from a displayed node element (view).
   *
   * @param Item item
   *        The item describing a target element.
   */
  _untangleItem: function (item) {
    if (item.finalize) {
      item.finalize(item);
    }
    for (let childItem of item) {
      item.remove(childItem);
    }

    this._unlinkItem(item);
    item._target = null;
  },

  /**
   * Deletes an item from the its parent's storage maps.
   *
   * @param Item item
   *        The item describing a target element.
   */
  _unlinkItem: function (item) {
    this._itemsByElement.delete(item._target);
  },

  /**
   * Returns a string representing the object.
   * Avoid using `toString` to avoid accidental JSONification.
   * @return string
   */
  stringify: function () {
    return JSON.stringify({
      value: this._value,
      target: this._target + "",
      prebuiltNode: this._prebuiltNode + "",
      attachment: this.attachment
    }, null, 2);
  },

  _value: "",
  _target: null,
  _prebuiltNode: null,
  finalize: null,
  attachment: null
};

/**
 * Some generic Widget methods handling Item instances.
 * Iterable via "for (let childItem of wrappedView) { }".
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
 * The devtools/shared/widgets/SimpleListWidget.jsm is an implementation
 * example.
 *
 * Language:
 *   - An "item" is an instance of an Item.
 *   - An "element" or "node" is a nsIDOMNode.
 *
 * The supplied widget can be any object implementing the following
 * methods:
 *   - function:nsIDOMNode insertItemAt(aIndex:number, aNode:nsIDOMNode,
 *                                      aValue:string)
 *   - function:nsIDOMNode getItemAtIndex(aIndex:number)
 *   - function removeChild(aChild:nsIDOMNode)
 *   - function removeAllItems()
 *   - get:nsIDOMNode selectedItem()
 *   - set selectedItem(aChild:nsIDOMNode)
 *   - function getAttribute(aName:string)
 *   - function setAttribute(aName:string, aValue:string)
 *   - function removeAttribute(aName:string)
 *   - function addEventListener(aName:string, aCallback:function,
 *                               aBubbleFlag:boolean)
 *   - function removeEventListener(aName:string, aCallback:function,
 *                                  aBubbleFlag:boolean)
 *
 * Optional methods that can be implemented by the widget:
 *   - function ensureElementIsVisible(aChild:nsIDOMNode)
 *
 * Optional attributes that may be handled (when calling
 * get/set/removeAttribute):
 *   - "emptyText": label temporarily added when there are no items present
 *   - "headerText": label permanently added as a header
 *
 * For automagical keyboard and mouse accessibility, the widget should be an
 * event emitter with the following events:
 *   - "keyPress" -> (aName:string, aEvent:KeyboardEvent)
 *   - "mousePress" -> (aName:string, aEvent:MouseEvent)
 */
const WidgetMethods = exports.WidgetMethods = {
  /**
   * Sets the element node or widget associated with this container.
   * @param nsIDOMNode | object widget
   */
  set widget(widget) {
    this._widget = widget;

    // Can't use a WeakMap for _itemsByValue because keys are strings, and
    // can't use one for _itemsByElement either, since it needs to be iterable.
    this._itemsByValue = new Map();
    this._itemsByElement = new Map();
    this._stagedItems = [];

    // Handle internal events emitted by the widget if necessary.
    if (ViewHelpers.isEventEmitter(widget)) {
      widget.on("keyPress", this._onWidgetKeyPress.bind(this));
      widget.on("mousePress", this._onWidgetMousePress.bind(this));
    }
  },

  /**
   * Gets the element node or widget associated with this container.
   * @return nsIDOMNode | object
   */
  get widget() {
    return this._widget;
  },

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
   * sorted by their value. This can be overridden with the "index" flag,
   * specifying on which position should an item be appended. The "staged" and
   * "index" flags are mutually exclusive, meaning that all staged items
   * will always be appended.
   *
   * @param nsIDOMNode element
   *        A prebuilt node to be wrapped.
   * @param string value
   *        A string identifying the node.
   * @param object options [optional]
   *        Additional options or flags supported by this operation:
   *          - attachment: some attached primitive/object for the item
   *          - staged: true to stage the item to be appended later
   *          - index: specifies on which position should the item be appended
   *          - attributes: a batch of attributes set to the displayed element
   *          - finalize: function invoked when the item is removed
   * @return Item
   *         The item associated with the displayed element if an unstaged push,
   *         undefined if the item was staged for a later commit.
   */
  push: function ([element, value], options = {}) {
    let item = new Item(this, element, value, options.attachment);

    // Batch the item to be added later.
    if (options.staged) {
      // An ulterior commit operation will ignore any specified index, so
      // no reason to keep it around.
      options.index = undefined;
      return void this._stagedItems.push({ item: item, options: options });
    }
    // Find the target position in this container and insert the item there.
    if (!("index" in options)) {
      return this._insertItemAt(this._findExpectedIndexFor(item), item,
                                options);
    }
    // Insert the item at the specified index. If negative or out of bounds,
    // the item will be simply appended.
    return this._insertItemAt(options.index, item, options);
  },

  /**
   * Flushes all the prepared items into this container.
   * Any specified index on the items will be ignored. Everything is appended.
   *
   * @param object options [optional]
   *        Additional options or flags supported by this operation:
   *          - sorted: true to sort all the items before adding them
   */
  commit: function (options = {}) {
    let stagedItems = this._stagedItems;

    // Sort the items before adding them to this container, if preferred.
    if (options.sorted) {
      stagedItems.sort((a, b) => this._currentSortPredicate(a.item, b.item));
    }
    // Append the prepared items to this container.
    for (let { item, opt } of stagedItems) {
      this._insertItemAt(-1, item, opt);
    }
    // Recreate the temporary items list for ulterior pushes.
    this._stagedItems.length = 0;
  },

  /**
   * Immediately removes the specified item from this container.
   *
   * @param Item item
   *        The item associated with the element to remove.
   */
  remove: function (item) {
    if (!item) {
      return;
    }
    this._widget.removeChild(item._target);
    this._untangleItem(item);

    if (!this._itemsByElement.size) {
      this._preferredValue = this.selectedValue;
      this._widget.selectedItem = null;
      this._widget.setAttribute("emptyText", this._emptyText);
    }
  },

  /**
   * Removes the item at the specified index from this container.
   *
   * @param number index
   *        The index of the item to remove.
   */
  removeAt: function (index) {
    this.remove(this.getItemAtIndex(index));
  },

  /**
   * Removes the items in this container based on a predicate.
   */
  removeForPredicate: function (predicate) {
    let item;
    while ((item = this.getItemForPredicate(predicate))) {
      this.remove(item);
    }
  },

  /**
   * Removes all items from this container.
   */
  empty: function () {
    this._preferredValue = this.selectedValue;
    this._widget.selectedItem = null;
    this._widget.removeAllItems();
    this._widget.setAttribute("emptyText", this._emptyText);

    for (let [, item] of this._itemsByElement) {
      this._untangleItem(item);
    }

    this._itemsByValue.clear();
    this._itemsByElement.clear();
    this._stagedItems.length = 0;
  },

  /**
   * Ensures the specified item is visible in this container.
   *
   * @param Item item
   *        The item to bring into view.
   */
  ensureItemIsVisible: function (item) {
    this._widget.ensureElementIsVisible(item._target);
  },

  /**
   * Ensures the item at the specified index is visible in this container.
   *
   * @param number index
   *        The index of the item to bring into view.
   */
  ensureIndexIsVisible: function (index) {
    this.ensureItemIsVisible(this.getItemAtIndex(index));
  },

  /**
   * Sugar for ensuring the selected item is visible in this container.
   */
  ensureSelectedItemIsVisible: function () {
    this.ensureItemIsVisible(this.selectedItem);
  },

  /**
   * If supported by the widget, the label string temporarily added to this
   * container when there are no child items present.
   */
  set emptyText(value) {
    this._emptyText = value;

    // Apply the emptyText attribute right now if there are no child items.
    if (!this._itemsByElement.size) {
      this._widget.setAttribute("emptyText", value);
    }
  },

  /**
   * If supported by the widget, the label string permanently added to this
   * container as a header.
   * @param string value
   */
  set headerText(value) {
    this._headerText = value;
    this._widget.setAttribute("headerText", value);
  },

  /**
   * Toggles all the items in this container hidden or visible.
   *
   * This does not change the default filtering predicate, so newly inserted
   * items will always be visible. Use WidgetMethods.filterContents if you care.
   *
   * @param boolean visibleFlag
   *        Specifies the intended visibility.
   */
  toggleContents: function (visibleFlag) {
    for (let [element] of this._itemsByElement) {
      element.hidden = !visibleFlag;
    }
  },

  /**
   * Toggles all items in this container hidden or visible based on a predicate.
   *
   * @param function predicate [optional]
   *        Items are toggled according to the return value of this function,
   *        which will become the new default filtering predicate in this
   *        container.
   *        If unspecified, all items will be toggled visible.
   */
  filterContents: function (predicate = this._currentFilterPredicate) {
    this._currentFilterPredicate = predicate;

    for (let [element, item] of this._itemsByElement) {
      element.hidden = !predicate(item);
    }
  },

  /**
   * Sorts all the items in this container based on a predicate.
   *
   * @param function predicate [optional]
   *        Items are sorted according to the return value of the function,
   *        which will become the new default sorting predicate in this
   *        container. If unspecified, all items will be sorted by their value.
   */
  sortContents: function (predicate = this._currentSortPredicate) {
    let sortedItems = this.items.sort(this._currentSortPredicate = predicate);

    for (let i = 0, len = sortedItems.length; i < len; i++) {
      this.swapItems(this.getItemAtIndex(i), sortedItems[i]);
    }
  },

  /**
   * Visually swaps two items in this container.
   *
   * @param Item first
   *        The first item to be swapped.
   * @param Item second
   *        The second item to be swapped.
   */
  swapItems: function (first, second) {
    if (first == second) {
      // We're just dandy, thank you.
      return;
    }
    let { _prebuiltNode: firstPrebuiltTarget, _target: firstTarget } = first;
    let { _prebuiltNode: secondPrebuiltTarget, _target: secondTarget } = second;

    // If the two items were constructed with prebuilt nodes as
    // DocumentFragments, then those DocumentFragments are now
    // empty and need to be reassembled.
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
    this._unlinkItem(first);
    this._unlinkItem(second);

    // 4. Add the items again, but reversing their indices.
    this._insertItemAt.apply(this, i < j ? [i, second] : [j, first]);
    this._insertItemAt.apply(this, i < j ? [j, first] : [i, second]);

    // 5. Restore the previous selection, if necessary.
    if (selectedIndex == i) {
      this._widget.selectedItem = first._target;
    } else if (selectedIndex == j) {
      this._widget.selectedItem = second._target;
    }

    // 6. Let the outside world know that these two items were swapped.
    ViewHelpers.dispatchEvent(first.target, "swap", [second, first]);
  },

  /**
   * Visually swaps two items in this container at specific indices.
   *
   * @param number first
   *        The index of the first item to be swapped.
   * @param number second
   *        The index of the second item to be swapped.
   */
  swapItemsAtIndices: function (first, second) {
    this.swapItems(this.getItemAtIndex(first), this.getItemAtIndex(second));
  },

  /**
   * Checks whether an item with the specified value is among the elements
   * shown in this container.
   *
   * @param string value
   *        The item's value.
   * @return boolean
   *         True if the value is known, false otherwise.
   */
  containsValue: function (value) {
    return this._itemsByValue.has(value) ||
           this._stagedItems.some(({ item }) => item._value == value);
  },

  /**
   * Gets the "preferred value". This is the latest selected item's value,
   * remembered just before emptying this container.
   * @return string
   */
  get preferredValue() {
    return this._preferredValue;
  },

  /**
   * Retrieves the item associated with the selected element.
   * @return Item | null
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
   * Retrieves the attachment of the selected element.
   * @return object | null
   */
  get selectedAttachment() {
    let selectedElement = this._widget.selectedItem;
    if (selectedElement) {
      return this._itemsByElement.get(selectedElement).attachment;
    }
    return null;
  },

  _selectItem: function (item) {
    // A falsy item is allowed to invalidate the current selection.
    let targetElement = item ? item._target : null;
    let prevElement = this._widget.selectedItem;

    // Make sure the selected item's target element is focused and visible.
    if (this.autoFocusOnSelection && targetElement) {
      targetElement.focus();
    }

    if (targetElement != prevElement) {
      this._widget.selectedItem = targetElement;
    }
  },

  /**
   * Selects the element with the entangled item in this container.
   * @param Item | function item
   */
  set selectedItem(item) {
    // A predicate is allowed to select a specific item.
    // If no item is matched, then the current selection is removed.
    if (typeof item == "function") {
      item = this.getItemForPredicate(item);
    }

    let targetElement = item ? item._target : null;
    let prevElement = this._widget.selectedItem;

    if (this.maintainSelectionVisible && targetElement) {
      // Some methods are optional. See the WidgetMethods object documentation
      // for a comprehensive list.
      if ("ensureElementIsVisible" in this._widget) {
        this._widget.ensureElementIsVisible(targetElement);
      }
    }

    this._selectItem(item);

    // Prevent selecting the same item again and avoid dispatching
    // a redundant selection event, so return early.
    if (targetElement != prevElement) {
      let dispTarget = targetElement || prevElement;
      let dispName = this.suppressSelectionEvents ? "suppressed-select"
                                                  : "select";
      ViewHelpers.dispatchEvent(dispTarget, dispName, item);
    }
  },

  /**
   * Selects the element at the specified index in this container.
   * @param number index
   */
  set selectedIndex(index) {
    let targetElement = this._widget.getItemAtIndex(index);
    if (targetElement) {
      this.selectedItem = this._itemsByElement.get(targetElement);
      return;
    }
    this.selectedItem = null;
  },

  /**
   * Selects the element with the specified value in this container.
   * @param string value
   */
  set selectedValue(value) {
    this.selectedItem = this._itemsByValue.get(value);
  },

  /**
   * Deselects and re-selects an item in this container.
   *
   * Useful when you want a "select" event to be emitted, even though
   * the specified item was already selected.
   *
   * @param Item | function item
   * @see `set selectedItem`
   */
  forceSelect: function (item) {
    this.selectedItem = null;
    this.selectedItem = item;
  },

  /**
   * Specifies if this container should try to keep the selected item visible.
   * (For example, when new items are added the selection is brought into view).
   */
  maintainSelectionVisible: true,

  /**
   * Specifies if "select" events dispatched from the elements in this container
   * when their respective items are selected should be suppressed or not.
   *
   * If this flag is set to true, then consumers of this container won't
   * be normally notified when items are selected.
   */
  suppressSelectionEvents: false,

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
   * When focusing on input, allow right clicks?
   * @see WidgetMethods.autoFocusOnInput
   */
  allowFocusOnRightClick: false,

  /**
   * The number of elements in this container to jump when Page Up or Page Down
   * keys are pressed. If falsy, then the page size will be based on the
   * number of visible items in the container.
   */
  pageSize: 0,

  /**
   * Focuses the first visible item in this container.
   */
  focusFirstVisibleItem: function () {
    this.focusItemAtDelta(-this.itemCount);
  },

  /**
   * Focuses the last visible item in this container.
   */
  focusLastVisibleItem: function () {
    this.focusItemAtDelta(+this.itemCount);
  },

  /**
   * Focuses the next item in this container.
   */
  focusNextItem: function () {
    this.focusItemAtDelta(+1);
  },

  /**
   * Focuses the previous item in this container.
   */
  focusPrevItem: function () {
    this.focusItemAtDelta(-1);
  },

  /**
   * Focuses another item in this container based on the index distance
   * from the currently focused item.
   *
   * @param number delta
   *        A scalar specifying by how many items should the selection change.
   */
  focusItemAtDelta: function (delta) {
    // Make sure the currently selected item is also focused, so that the
    // command dispatcher mechanism has a relative node to work with.
    // If there's no selection, just select an item at a corresponding index
    // (e.g. the first item in this container if delta <= 1).
    let selectedElement = this._widget.selectedItem;
    if (selectedElement) {
      selectedElement.focus();
    } else {
      this.selectedIndex = Math.max(0, delta - 1);
      return;
    }

    let direction = delta > 0 ? "advanceFocus" : "rewindFocus";
    let distance = Math.abs(Math[delta > 0 ? "ceil" : "floor"](delta));
    while (distance--) {
      if (!this._focusChange(direction)) {
        // Out of bounds.
        break;
      }
    }

    // Synchronize the selected item as being the currently focused element.
    this.selectedItem = this.getItemForElement(this._focusedElement);
  },

  /**
   * Focuses the next or previous item in this container.
   *
   * @param string direction
   *        Either "advanceFocus" or "rewindFocus".
   * @return boolean
   *         False if the focus went out of bounds and the first or last item
   *         in this container was focused instead.
   */
  _focusChange: function (direction) {
    let commandDispatcher = this._commandDispatcher;
    let prevFocusedElement = commandDispatcher.focusedElement;
    let currFocusedElement;

    do {
      commandDispatcher.suppressFocusScroll = true;
      commandDispatcher[direction]();
      currFocusedElement = commandDispatcher.focusedElement;

      // Make sure the newly focused item is a part of this container. If the
      // focus goes out of bounds, revert the previously focused item.
      if (!this.getItemForElement(currFocusedElement)) {
        prevFocusedElement.focus();
        return false;
      }
    } while (!WIDGET_FOCUSABLE_NODES.has(currFocusedElement.tagName));

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
      this._cachedCommandDispatcher = commandDispatcher;
      return commandDispatcher;
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
   * @param number index
   *        The index used to identify the element.
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemAtIndex: function (index) {
    return this.getItemForElement(this._widget.getItemAtIndex(index));
  },

  /**
   * Gets the item in the container having the specified value.
   *
   * @param string value
   *        The value used to identify the element.
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemByValue: function (value) {
    return this._itemsByValue.get(value);
  },

  /**
   * Gets the item in the container associated with the specified element.
   *
   * @param nsIDOMNode element
   *        The element used to identify the item.
   * @param object flags [optional]
   *        Additional options for showing the source. Supported options:
   *          - noSiblings: if siblings shouldn't be taken into consideration
   *                        when searching for the associated item.
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemForElement: function (element, flags = {}) {
    while (element) {
      let item = this._itemsByElement.get(element);

      // Also search the siblings if allowed.
      if (!flags.noSiblings) {
        item = item ||
          this._itemsByElement.get(element.nextElementSibling) ||
          this._itemsByElement.get(element.previousElementSibling);
      }
      if (item) {
        return item;
      }
      element = element.parentNode;
    }
    return null;
  },

  /**
   * Gets a visible item in this container validating a specified predicate.
   *
   * @param function predicate
   *        The first item which validates this predicate is returned
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemForPredicate: function (predicate, owner = this) {
    // Recursively check the items in this widget for a predicate match.
    for (let [element, item] of owner._itemsByElement) {
      let match;
      if (predicate(item) && !element.hidden) {
        match = item;
      } else {
        match = this.getItemForPredicate(predicate, item);
      }
      if (match) {
        return match;
      }
    }
    // Also check the staged items. No need to do this recursively since
    // they're not even appended to the view yet.
    for (let { item } of this._stagedItems) {
      if (predicate(item)) {
        return item;
      }
    }
    return null;
  },

  /**
   * Shortcut function for getItemForPredicate which works on item attachments.
   * @see getItemForPredicate
   */
  getItemForAttachment: function (predicate, owner = this) {
    return this.getItemForPredicate(e => predicate(e.attachment));
  },

  /**
   * Finds the index of an item in the container.
   *
   * @param Item item
   *        The item get the index for.
   * @return number
   *         The index of the matched item, or -1 if nothing is found.
   */
  indexOfItem: function (item) {
    return this._indexOfElement(item._target);
  },

  /**
   * Finds the index of an element in the container.
   *
   * @param nsIDOMNode element
   *        The element get the index for.
   * @return number
   *         The index of the matched element, or -1 if nothing is found.
   */
  _indexOfElement: function (element) {
    for (let i = 0; i < this._itemsByElement.size; i++) {
      if (this._widget.getItemAtIndex(i) == element) {
        return i;
      }
    }
    return -1;
  },

  /**
   * Gets the total number of items in this container.
   * @return number
   */
  get itemCount() {
    return this._itemsByElement.size;
  },

  /**
   * Returns a list of items in this container, in the displayed order.
   * @return array
   */
  get items() {
    let store = [];
    let itemCount = this.itemCount;
    for (let i = 0; i < itemCount; i++) {
      store.push(this.getItemAtIndex(i));
    }
    return store;
  },

  /**
   * Returns a list of values in this container, in the displayed order.
   * @return array
   */
  get values() {
    return this.items.map(e => e._value);
  },

  /**
   * Returns a list of attachments in this container, in the displayed order.
   * @return array
   */
  get attachments() {
    return this.items.map(e => e.attachment);
  },

  /**
   * Returns a list of all the visible (non-hidden) items in this container,
   * in the displayed order
   * @return array
   */
  get visibleItems() {
    return this.items.filter(e => !e._target.hidden);
  },

  /**
   * Checks if an item is unique in this container. If an item's value is an
   * empty string, "undefined" or "null", it is considered unique.
   *
   * @param Item item
   *        The item for which to verify uniqueness.
   * @return boolean
   *         True if the item is unique, false otherwise.
   */
  isUnique: function (item) {
    let value = item._value;
    if (value == "" || value == "undefined" || value == "null") {
      return true;
    }
    return !this._itemsByValue.has(value);
  },

  /**
   * Checks if an item is eligible for this container. By default, this checks
   * whether an item is unique and has a prebuilt target node.
   *
   * @param Item item
   *        The item for which to verify eligibility.
   * @return boolean
   *         True if the item is eligible, false otherwise.
   */
  isEligible: function (item) {
    return this.isUnique(item) && item._prebuiltNode;
  },

  /**
   * Finds the expected item index in this container based on the default
   * sort predicate.
   *
   * @param Item item
   *        The item for which to get the expected index.
   * @return number
   *         The expected item index.
   */
  _findExpectedIndexFor: function (item) {
    let itemCount = this.itemCount;
    for (let i = 0; i < itemCount; i++) {
      if (this._currentSortPredicate(this.getItemAtIndex(i), item) > 0) {
        return i;
      }
    }
    return itemCount;
  },

  /**
   * Immediately inserts an item in this container at the specified index.
   *
   * @param number index
   *        The position in the container intended for this item.
   * @param Item item
   *        The item describing a target element.
   * @param object options [optional]
   *        Additional options or flags supported by this operation:
   *          - attributes: a batch of attributes set to the displayed element
   *          - finalize: function when the item is untangled (removed)
   * @return Item
   *         The item associated with the displayed element, null if rejected.
   */
  _insertItemAt: function (index, item, options = {}) {
    if (!this.isEligible(item)) {
      return null;
    }

    // Entangle the item with the newly inserted node.
    // Make sure this is done with the value returned by insertItemAt(),
    // to avoid storing a potential DocumentFragment.
    let node = item._prebuiltNode;
    let attachment = item.attachment;
    this._entangleItem(item,
                       this._widget.insertItemAt(index, node, attachment));

    // Handle any additional options after entangling the item.
    if (!this._currentFilterPredicate(item)) {
      item._target.hidden = true;
    }
    if (this.autoFocusOnFirstItem && this._itemsByElement.size == 1) {
      item._target.focus();
    }
    if (options.attributes) {
      options.attributes.forEach(e => item._target.setAttribute(e[0], e[1]));
    }
    if (options.finalize) {
      item.finalize = options.finalize;
    }

    // Hide the empty text if the selection wasn't lost.
    this._widget.removeAttribute("emptyText");

    // Return the item associated with the displayed element.
    return item;
  },

  /**
   * Entangles an item (model) with a displayed node element (view).
   *
   * @param Item item
   *        The item describing a target element.
   * @param nsIDOMNode element
   *        The element displaying the item.
   */
  _entangleItem: function (item, element) {
    this._itemsByValue.set(item._value, item);
    this._itemsByElement.set(element, item);
    item._target = element;
  },

  /**
   * Untangles an item (model) from a displayed node element (view).
   *
   * @param Item item
   *        The item describing a target element.
   */
  _untangleItem: function (item) {
    if (item.finalize) {
      item.finalize(item);
    }
    for (let childItem of item) {
      item.remove(childItem);
    }

    this._unlinkItem(item);
    item._target = null;
  },

  /**
   * Deletes an item from the its parent's storage maps.
   *
   * @param Item item
   *        The item describing a target element.
   */
  _unlinkItem: function (item) {
    this._itemsByValue.delete(item._value);
    this._itemsByElement.delete(item._target);
  },

  /**
   * The keyPress event listener for this container.
   * @param string name
   * @param KeyboardEvent event
   */
  _onWidgetKeyPress: function (name, event) {
    // Prevent scrolling when pressing navigation keys.
    ViewHelpers.preventScrolling(event);

    switch (event.keyCode) {
      case event.DOM_VK_UP:
      case event.DOM_VK_LEFT:
        this.focusPrevItem();
        return;
      case event.DOM_VK_DOWN:
      case event.DOM_VK_RIGHT:
        this.focusNextItem();
        return;
      case event.DOM_VK_PAGE_UP:
        this.focusItemAtDelta(-(this.pageSize ||
                               (this.itemCount / PAGE_SIZE_ITEM_COUNT_RATIO)));
        return;
      case event.DOM_VK_PAGE_DOWN:
        this.focusItemAtDelta(+(this.pageSize ||
                               (this.itemCount / PAGE_SIZE_ITEM_COUNT_RATIO)));
        return;
      case event.DOM_VK_HOME:
        this.focusFirstVisibleItem();
        return;
      case event.DOM_VK_END:
        this.focusLastVisibleItem();
        return;
    }
  },

  /**
   * The mousePress event listener for this container.
   * @param string name
   * @param MouseEvent event
   */
  _onWidgetMousePress: function (name, event) {
    if (event.button != 0 && !this.allowFocusOnRightClick) {
      // Only allow left-click to trigger this event.
      return;
    }

    let item = this.getItemForElement(event.target);
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
   * @param Item item
   *        The item passing through the filter.
   * @return boolean
   *         True if the item should be visible, false otherwise.
   */
  _currentFilterPredicate: function (item) {
    return true;
  },

  /**
   * The predicate used when sorting items. By default, items in this view
   * are sorted by their label.
   *
   * @param Item first
   *        The first item used in the comparison.
   * @param Item second
   *        The second item used in the comparison.
   * @return number
   *         -1 to sort first to a lower index than second
   *          0 to leave first and second unchanged with respect to each other
   *          1 to sort second to a lower index than first
   */
  _currentSortPredicate: function (first, second) {
    return +(first._value.toLowerCase() > second._value.toLowerCase());
  },

  /**
   * Call a method on this widget named `methodName`. Any further arguments are
   * passed on to the method. Returns the result of the method call.
   *
   * @param String methodName
   *        The name of the method you want to call.
   * @param args
   *        Optional. Any arguments you want to pass through to the method.
   */
  callMethod: function (methodName, ...args) {
    return this._widget[methodName].apply(this._widget, args);
  },

  _widget: null,
  _emptyText: "",
  _headerText: "",
  _preferredValue: "",
  _cachedCommandDispatcher: null
};

/**
 * A generator-iterator over all the items in this container.
 */
Item.prototype[Symbol.iterator] =
WidgetMethods[Symbol.iterator] = function* () {
  yield* this._itemsByElement.values();
};
