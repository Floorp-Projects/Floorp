/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
const WIDGET_FOCUSABLE_NODES = new Set(["vbox", "hbox"]);

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
const { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
Cu.import("resource://gre/modules/devtools/event-emitter.js");

this.EXPORTED_SYMBOLS = [
  "Heritage", "ViewHelpers", "WidgetMethods",
  "setNamedTimeout", "clearNamedTimeout",
  "setConditionalTimeout", "clearConditionalTimeout",
];

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
 * Helper for draining a rapid succession of events and invoking a callback
 * once everything settles down.
 *
 * @param string aId
 *        A string identifier for the named timeout.
 * @param number aWait
 *        The amount of milliseconds to wait after no more events are fired.
 * @param function aCallback
 *        Invoked when no more events are fired after the specified time.
 */
this.setNamedTimeout = function setNamedTimeout(aId, aWait, aCallback) {
  clearNamedTimeout(aId);

  namedTimeoutsStore.set(aId, setTimeout(() =>
    namedTimeoutsStore.delete(aId) && aCallback(), aWait));
};

/**
 * Clears a named timeout.
 * @see setNamedTimeout
 *
 * @param string aId
 *        A string identifier for the named timeout.
 */
this.clearNamedTimeout = function clearNamedTimeout(aId) {
  if (!namedTimeoutsStore) {
    return;
  }
  clearTimeout(namedTimeoutsStore.get(aId));
  namedTimeoutsStore.delete(aId);
};

/**
 * Same as `setNamedTimeout`, but invokes the callback only if the provided
 * predicate function returns true. Otherwise, the timeout is re-triggered.
 *
 * @param string aId
 *        A string identifier for the conditional timeout.
 * @param number aWait
 *        The amount of milliseconds to wait after no more events are fired.
 * @param function aPredicate
 *        The predicate function used to determine whether the timeout restarts.
 * @param function aCallback
 *        Invoked when no more events are fired after the specified time, and
 *        the provided predicate function returns true.
 */
this.setConditionalTimeout = function setConditionalTimeout(aId, aWait, aPredicate, aCallback) {
  setNamedTimeout(aId, aWait, function maybeCallback() {
    if (aPredicate()) {
      aCallback();
      return;
    }
    setConditionalTimeout(aId, aWait, aPredicate, aCallback);
  });
};

/**
 * Clears a conditional timeout.
 * @see setConditionalTimeout
 *
 * @param string aId
 *        A string identifier for the conditional timeout.
 */
this.clearConditionalTimeout = function clearConditionalTimeout(aId) {
  clearNamedTimeout(aId);
};

XPCOMUtils.defineLazyGetter(this, "namedTimeoutsStore", () => new Map());

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
    aWidget.getAttribute =
      aWidget.getAttribute || aNode.getAttribute.bind(aNode);
    aWidget.setAttribute =
      aWidget.setAttribute || aNode.setAttribute.bind(aNode);
    aWidget.removeAttribute =
      aWidget.removeAttribute || aNode.removeAttribute.bind(aNode);
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
    aWidget.addEventListener =
      aWidget.addEventListener || aNode.addEventListener.bind(aNode);
    aWidget.removeEventListener =
      aWidget.removeEventListener || aNode.removeEventListener.bind(aNode);
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
   * Checks if the specified object is an instance of a DOM node.
   *
   * @return boolean
   *         True if it's a node, false otherwise.
   */
  isNode: function(aObject) {
    return aObject instanceof Ci.nsIDOMNode ||
           aObject instanceof Ci.nsIDOMElement ||
           aObject instanceof Ci.nsIDOMDocumentFragment;
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
    // Make sure a pane is actually available first.
    if (!aPane) {
      return;
    }

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

    // The "animated" attributes enables animated toggles (slide in-out).
    if (aFlags.animated) {
      aPane.setAttribute("animated", "");
    } else {
      aPane.removeAttribute("animated");
    }

    // Computes and sets the pane margins in order to hide or show it.
    let doToggle = () => {
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

    // Sometimes it's useful delaying the toggle a few ticks to ensure
    // a smoother slide in-out animation.
    if (aFlags.delayed) {
      aPane.ownerDocument.defaultView.setTimeout(doToggle, PANE_APPEARANCE_DELAY);
    } else {
      doToggle();
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
    if (isNaN(aNumber) || aNumber == null) {
      return "0";
    }
    let localized = aNumber.toLocaleString(); // localize

    // If no grouping or decimal separators are available, bail out, because
    // padding with zeros at the end of the string won't make sense anymore.
    if (!localized.match(/[^\d]/)) {
      return localized;
    }

    return aNumber.toLocaleString(undefined, {
      maximumFractionDigits: aDecimals,
      minimumFractionDigits: aDecimals
    });
  }
};

/**
 * A helper for having the same interface as ViewHelpers.L10N, but for
 * more than one file. Useful for abstracting l10n string locations.
 */
ViewHelpers.MultiL10N = function(aStringBundleNames) {
  let l10ns = aStringBundleNames.map(bundle => new ViewHelpers.L10N(bundle));
  let proto = ViewHelpers.L10N.prototype;

  Object.getOwnPropertyNames(proto)
    .map(name => ({
      name: name,
      desc: Object.getOwnPropertyDescriptor(proto, name)
    }))
    .filter(property => property.desc.value instanceof Function)
    .forEach(method => {
      this[method.name] = function(...args) {
        for (let l10n of l10ns) {
          try { return method.desc.value.apply(l10n, args) } catch (e) {}
        }
      };
    });
};

/**
 * Shortcuts for lazily accessing and setting various preferences.
 * Usage:
 *   let prefs = new ViewHelpers.Prefs("root.path.to.branch", {
 *     myIntPref: ["Int", "leaf.path.to.my-int-pref"],
 *     myCharPref: ["Char", "leaf.path.to.my-char-pref"],
 *     myJsonPref: ["Json", "leaf.path.to.my-json-pref"],
 *     myFloatPref: ["Float", "leaf.path.to.my-float-pref"]
 *     ...
 *   });
 *
 * Get/set:
 *   prefs.myCharPref = "foo";
 *   let aux = prefs.myCharPref;
 *
 * Observe:
 *   prefs.registerObserver();
 *   prefs.on("pref-changed", (prefName, prefValue) => {
 *     ...
 *   });
 *
 * @param string aPrefsRoot
 *        The root path to the required preferences branch.
 * @param object aPrefsBlueprint
 *        An object containing { accessorName: [prefType, prefName] } keys.
 * @param object aOptions
 *        Additional options for this constructor. Currently supported:
 *          - monitorChanges: true to update the stored values if they changed
 *                            when somebody edits about:config or the prefs
 *                            change somewhere else.
 */
ViewHelpers.Prefs = function(aPrefsRoot = "", aPrefsBlueprint = {}, aOptions = {}) {
  EventEmitter.decorate(this);

  this._cache = new Map();
  let self = this;

  for (let [accessorName, [prefType, prefName]] of Iterator(aPrefsBlueprint)) {
    this._map(accessorName, prefType, aPrefsRoot, prefName);
  }

  let observer = {
    register: function() {
      this.branch = Services.prefs.getBranch(aPrefsRoot + ".");
      this.branch.addObserver("", this, false);
    },
    unregister: function() {
      this.branch.removeObserver("", this);
    },
    observe: function(_, __, aPrefName) {
      // If this particular pref isn't handled by the blueprint object,
      // even though it's in the specified branch, ignore it.
      let accessor = self._accessor(aPrefsBlueprint, aPrefName);
      if (!(accessor in self)) {
        return;
      }
      self._cache.delete(aPrefName);
      self.emit("pref-changed", accessor, self[accessor]);
    }
  };

  this.registerObserver = () => observer.register();
  this.unregisterObserver = () => observer.unregister();

  if (aOptions.monitorChanges) {
    this.registerObserver();
  }
};

ViewHelpers.Prefs.prototype = {
  /**
   * Helper method for getting a pref value.
   *
   * @param string aType
   * @param string aPrefsRoot
   * @param string aPrefName
   * @return any
   */
  _get: function(aType, aPrefsRoot, aPrefName) {
    let cachedPref = this._cache.get(aPrefName);
    if (cachedPref !== undefined) {
      return cachedPref;
    }
    let value = Services.prefs["get" + aType + "Pref"]([aPrefsRoot, aPrefName].join("."));
    this._cache.set(aPrefName, value);
    return value;
  },

  /**
   * Helper method for setting a pref value.
   *
   * @param string aType
   * @param string aPrefsRoot
   * @param string aPrefName
   * @param any aValue
   */
  _set: function(aType, aPrefsRoot, aPrefName, aValue) {
    Services.prefs["set" + aType + "Pref"]([aPrefsRoot, aPrefName].join("."), aValue);
    this._cache.set(aPrefName, aValue);
  },

  /**
   * Maps a property name to a pref, defining lazy getters and setters.
   * Supported types are "Bool", "Char", "Int", "Float" (sugar around "Char" type and casting),
   * and "Json" (which is basically just sugar for "Char" using the standard JSON serializer).
   *
   * @param string aAccessorName
   * @param string aType
   * @param string aPrefsRoot
   * @param string aPrefName
   * @param array aSerializer
   */
  _map: function(aAccessorName, aType, aPrefsRoot, aPrefName, aSerializer = { in: e => e, out: e => e }) {
    if (aPrefName in this) {
      throw new Error(`Can't use ${aPrefName} because it's already a property.`);
    }
    if (aType == "Json") {
      this._map(aAccessorName, "Char", aPrefsRoot, aPrefName, { in: JSON.parse, out: JSON.stringify });
      return;
    }
    if (aType == "Float") {
      this._map(aAccessorName, "Char", aPrefsRoot, aPrefName, { in: Number.parseFloat, out: (n) => n + ""});
      return;
    }

    Object.defineProperty(this, aAccessorName, {
      get: () => aSerializer.in(this._get(aType, aPrefsRoot, aPrefName)),
      set: (e) => this._set(aType, aPrefsRoot, aPrefName, aSerializer.out(e))
    });
  },

  /**
   * Finds the accessor in this object for the provided property name,
   * based on the blueprint object used in the constructor.
   */
  _accessor: function(aPrefsBlueprint, aPrefName) {
    for (let [accessorName, [, prefName]] of Iterator(aPrefsBlueprint)) {
      if (prefName == aPrefName) {
        return accessorName;
      }
    }
    return null;
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
 * @param object aOwnerView
 *        The owner view creating this item.
 * @param nsIDOMNode aElement
 *        A prebuilt node to be wrapped.
 * @param string aValue
 *        A string identifying the node.
 * @param any aAttachment
 *        Some attached primitive/object.
 */
function Item(aOwnerView, aElement, aValue, aAttachment) {
  this.ownerView = aOwnerView;
  this.attachment = aAttachment;
  this._value = aValue + "";
  this._prebuiltNode = aElement;
};

Item.prototype = {
  get value() { return this._value; },
  get target() { return this._target; },
  get prebuiltNode() { return this._prebuiltNode; },

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
    let item = new Item(this, aElement, "", aOptions.attachment);

    // Entangle the item with the newly inserted child node.
    // Make sure this is done with the value returned by appendChild(),
    // to avoid storing a potential DocumentFragment.
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
    for (let childItem of aItem) {
      aItem.remove(childItem);
    }

    this._unlinkItem(aItem);
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
   * Avoid using `toString` to avoid accidental JSONification.
   * @return string
   */
  stringify: function() {
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

// Creating maps thousands of times for widgets with a large number of children
// fills up a lot of memory. Make sure these are instantiated only if needed.
DevToolsUtils.defineLazyPrototypeGetter(Item.prototype, "_itemsByElement", () => new Map());

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
 * The devtools/shared/widgets/SimpleListWidget.jsm is an implementation example.
 *
 * Language:
 *   - An "item" is an instance of an Item.
 *   - An "element" or "node" is a nsIDOMNode.
 *
 * The supplied widget can be any object implementing the following methods:
 *   - function:nsIDOMNode insertItemAt(aIndex:number, aNode:nsIDOMNode, aValue:string)
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
 * Optional methods that can be implemented by the widget:
 *   - function ensureElementIsVisible(aChild:nsIDOMNode)
 *
 * Optional attributes that may be handled (when calling get/set/removeAttribute):
 *   - "emptyText": label temporarily added when there are no items present
 *   - "headerText": label permanently added as a header
 *
 * For automagical keyboard and mouse accessibility, the widget should be an
 * event emitter with the following events:
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


    // Can't use a WeakMap for _itemsByValue because keys are strings, and
    // can't use one for _itemsByElement either, since it needs to be iterable.
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
   * sorted by their value. This can be overridden with the "index" flag,
   * specifying on which position should an item be appended. The "staged" and
   * "index" flags are mutually exclusive, meaning that all staged items
   * will always be appended.
   *
   * @param nsIDOMNode aElement
   *        A prebuilt node to be wrapped.
   * @param string aValue
   *        A string identifying the node.
   * @param object aOptions [optional]
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
  push: function([aElement, aValue], aOptions = {}) {
    let item = new Item(this, aElement, aValue, aOptions.attachment);

    // Batch the item to be added later.
    if (aOptions.staged) {
      // An ulterior commit operation will ignore any specified index, so
      // no reason to keep it around.
      aOptions.index = undefined;
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

    if (!this._itemsByElement.size) {
      this._preferredValue = this.selectedValue;
      this._widget.selectedItem = null;
      this._widget.setAttribute("emptyText", this._emptyText);
    }
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
   * Removes the items in this container based on a predicate.
   */
  removeForPredicate: function(aPredicate) {
    let item;
    while ((item = this.getItemForPredicate(aPredicate))) {
      this.remove(item);
    }
  },

  /**
   * Removes all items from this container.
   */
  empty: function() {
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
   * @param Item aItem
   *        The item to bring into view.
   */
  ensureItemIsVisible: function(aItem) {
    this._widget.ensureElementIsVisible(aItem._target);
  },

  /**
   * Ensures the item at the specified index is visible in this container.
   *
   * @param number aIndex
   *        The index of the item to bring into view.
   */
  ensureIndexIsVisible: function(aIndex) {
    this.ensureItemIsVisible(this.getItemAtIndex(aIndex));
  },

  /**
   * Sugar for ensuring the selected item is visible in this container.
   */
  ensureSelectedItemIsVisible: function() {
    this.ensureItemIsVisible(this.selectedItem);
  },

  /**
   * If supported by the widget, the label string temporarily added to this
   * container when there are no child items present.
   */
  set emptyText(aValue) {
    this._emptyText = aValue;

    // Apply the emptyText attribute right now if there are no child items.
    if (!this._itemsByElement.size) {
      this._widget.setAttribute("emptyText", aValue);
    }
  },

  /**
   * If supported by the widget, the label string permanently added to this
   * container as a header.
   * @param string aValue
   */
  set headerText(aValue) {
    this._headerText = aValue;
    this._widget.setAttribute("headerText", aValue);
  },

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
   *        If unspecified, all items will be sorted by their value.
   */
  sortContents: function(aPredicate = this._currentSortPredicate) {
    let sortedItems = this.items.sort(this._currentSortPredicate = aPredicate);

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
    let { _prebuiltNode: firstPrebuiltTarget, _target: firstTarget } = aFirst;
    let { _prebuiltNode: secondPrebuiltTarget, _target: secondTarget } = aSecond;

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

    // 6. Let the outside world know that these two items were swapped.
    ViewHelpers.dispatchEvent(aFirst.target, "swap", [aSecond, aFirst]);
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

    // Make sure the selected item's target element is focused and visible.
    if (this.autoFocusOnSelection && targetElement) {
      targetElement.focus();
    }
    if (this.maintainSelectionVisible && targetElement) {
      // Some methods are optional. See the WidgetMethods object documentation
      // for a comprehensive list.
      if ("ensureElementIsVisible" in this._widget) {
        this._widget.ensureElementIsVisible(targetElement);
      }
    }

    // Prevent selecting the same item again and avoid dispatching
    // a redundant selection event, so return early.
    if (targetElement != prevElement) {
      this._widget.selectedItem = targetElement;
      let dispTarget = targetElement || prevElement;
      let dispName = this.suppressSelectionEvents ? "suppressed-select" : "select";
      ViewHelpers.dispatchEvent(dispTarget, dispName, aItem);
    }
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
   * Selects the element with the specified value in this container.
   * @param string aValue
   */
  set selectedValue(aValue) {
    this.selectedItem = this._itemsByValue.get(aValue);
  },

  /**
   * Deselects and re-selects an item in this container.
   *
   * Useful when you want a "select" event to be emitted, even though
   * the specified item was already selected.
   *
   * @param Item | function aItem
   * @see `set selectedItem`
   */
  forceSelect: function(aItem) {
    this.selectedItem = null;
    this.selectedItem = aItem;
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
    let currFocusedElement;

    do {
      commandDispatcher.suppressFocusScroll = true;
      commandDispatcher[aDirection]();
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
   * @param object aFlags [optional]
   *        Additional options for showing the source. Supported options:
   *          - noSiblings: if siblings shouldn't be taken into consideration
   *                        when searching for the associated item.
   * @return Item
   *         The matched item, or null if nothing is found.
   */
  getItemForElement: function(aElement, aFlags = {}) {
    while (aElement) {
      let item = this._itemsByElement.get(aElement);

      // Also search the siblings if allowed.
      if (!aFlags.noSiblings) {
        item = item ||
          this._itemsByElement.get(aElement.nextElementSibling) ||
          this._itemsByElement.get(aElement.previousElementSibling);
      }
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
    // Recursively check the items in this widget for a predicate match.
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
    // Also check the staged items. No need to do this recursively since
    // they're not even appended to the view yet.
    for (let { item } of this._stagedItems) {
      if (aPredicate(item)) {
        return item;
      }
    }
    return null;
  },

  /**
   * Shortcut function for getItemForPredicate which works on item attachments.
   * @see getItemForPredicate
   */
  getItemForAttachment: function(aPredicate, aOwner = this) {
    return this.getItemForPredicate(e => aPredicate(e.attachment));
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
   * @param Item aItem
   *        The item for which to verify uniqueness.
   * @return boolean
   *         True if the item is unique, false otherwise.
   */
  isUnique: function(aItem) {
    let value = aItem._value;
    if (value == "" || value == "undefined" || value == "null") {
      return true;
    }
    return !this._itemsByValue.has(value);
  },

  /**
   * Checks if an item is eligible for this container. By default, this checks
   * whether an item is unique and has a prebuilt target node.
   *
   * @param Item aItem
   *        The item for which to verify eligibility.
   * @return boolean
   *         True if the item is eligible, false otherwise.
   */
  isEligible: function(aItem) {
    return this.isUnique(aItem) && aItem._prebuiltNode;
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
   *        The item describing a target element.
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - attributes: a batch of attributes set to the displayed element
   *          - finalize: function when the item is untangled (removed)
   * @return Item
   *         The item associated with the displayed element, null if rejected.
   */
  _insertItemAt: function(aIndex, aItem, aOptions = {}) {
    if (!this.isEligible(aItem)) {
      return null;
    }

    // Entangle the item with the newly inserted node.
    // Make sure this is done with the value returned by insertItemAt(),
    // to avoid storing a potential DocumentFragment.
    let node = aItem._prebuiltNode;
    let attachment = aItem.attachment;
    this._entangleItem(aItem, this._widget.insertItemAt(aIndex, node, attachment));

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

    // Hide the empty text if the selection wasn't lost.
    this._widget.removeAttribute("emptyText");

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
    for (let childItem of aItem) {
      aItem.remove(childItem);
    }

    this._unlinkItem(aItem);
    aItem._target = null;
  },

  /**
   * Deletes an item from the its parent's storage maps.
   *
   * @param Item aItem
   *        The item describing a target element.
   */
  _unlinkItem: function(aItem) {
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
   * The mousePress event listener for this container.
   * @param string aName
   * @param MouseEvent aEvent
   */
  _onWidgetMousePress: function(aName, aEvent) {
    if (aEvent.button != 0 && !this.allowFocusOnRightClick) {
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
    return +(aFirst._value.toLowerCase() > aSecond._value.toLowerCase());
  },

  /**
   * Call a method on this widget named `aMethodName`. Any further arguments are
   * passed on to the method. Returns the result of the method call.
   *
   * @param String aMethodName
   *        The name of the method you want to call.
   * @param aArgs
   *        Optional. Any arguments you want to pass through to the method.
   */
  callMethod: function(aMethodName, ...aArgs) {
    return this._widget[aMethodName].apply(this._widget, aArgs);
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
WidgetMethods[Symbol.iterator] = function*() {
  yield* this._itemsByElement.values();
};
