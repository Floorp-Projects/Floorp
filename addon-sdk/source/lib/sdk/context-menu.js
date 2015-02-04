/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "stable",
  "engines": {
    // TODO Fennec support Bug 788334
    "Firefox": "*",
    "SeaMonkey": "*"
  }
};

const { Class, mix } = require("./core/heritage");
const { addCollectionProperty } = require("./util/collection");
const { ns } = require("./core/namespace");
const { validateOptions, getTypeOf } = require("./deprecated/api-utils");
const { URL, isValidURI } = require("./url");
const { WindowTracker, browserWindowIterator } = require("./deprecated/window-utils");
const { isBrowser, getInnerId } = require("./window/utils");
const { Ci, Cc, Cu } = require("chrome");
const { MatchPattern } = require("./util/match-pattern");
const { EventTarget } = require("./event/target");
const { emit } = require('./event/core');
const { when } = require('./system/unload');
const { contract: loaderContract } = require('./content/loader');
const { omit } = require('./util/object');
const self = require('./self')

// null-out cycles in .modules to make @loader/options JSONable
const ADDON = omit(require('@loader/options'), ['modules', 'globals']);

require('../framescript/FrameScriptManager.jsm').enableCMEvents();

// All user items we add have this class.
const ITEM_CLASS = "addon-context-menu-item";

// Items in the top-level context menu also have this class.
const TOPLEVEL_ITEM_CLASS = "addon-context-menu-item-toplevel";

// Items in the overflow submenu also have this class.
const OVERFLOW_ITEM_CLASS = "addon-context-menu-item-overflow";

// The class of the menu separator that separates standard context menu items
// from our user items.
const SEPARATOR_CLASS = "addon-context-menu-separator";

// If more than this number of items are added to the context menu, all items
// overflow into a "Jetpack" submenu.
const OVERFLOW_THRESH_DEFAULT = 10;
const OVERFLOW_THRESH_PREF =
  "extensions.addon-sdk.context-menu.overflowThreshold";

// The label of the overflow sub-xul:menu.
//
// TODO: Localize these.
const OVERFLOW_MENU_LABEL = "Add-ons";
const OVERFLOW_MENU_ACCESSKEY = "A";

// The class of the overflow sub-xul:menu.
const OVERFLOW_MENU_CLASS = "addon-content-menu-overflow-menu";

// The class of the overflow submenu's xul:menupopup.
const OVERFLOW_POPUP_CLASS = "addon-content-menu-overflow-popup";

// Holds private properties for API objects
let internal = ns();

function uuid() {
  return require('./util/uuid').uuid().toString();
}

function getScheme(spec) {
  try {
    return URL(spec).scheme;
  }
  catch(e) {
    return null;
  }
}

let MessageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                     getService(Ci.nsIMessageBroadcaster);

let Context = Class({
  initialize: function() {
    internal(this).id = uuid();
  },

  // Returns the node that made this context current
  adjustPopupNode: function adjustPopupNode(popupNode) {
    return popupNode;
  },

  // Returns whether this context is current for the current node
  isCurrent: function isCurrent(state) {
    return state;
  }
});

// Matches when the context-clicked node doesn't have any of
// NON_PAGE_CONTEXT_ELTS in its ancestors
let PageContext = Class({
  extends: Context,

  serialize: function() {
    return {
      id: internal(this).id,
      type: "PageContext",
      args: []
    }
  }
});
exports.PageContext = PageContext;

// Matches when there is an active selection in the window
let SelectionContext = Class({
  extends: Context,

  serialize: function() {
    return {
      id: internal(this).id,
      type: "SelectionContext",
      args: []
    }
  }
});
exports.SelectionContext = SelectionContext;

// Matches when the context-clicked node or any of its ancestors matches the
// selector given
let SelectorContext = Class({
  extends: Context,

  initialize: function initialize(selector) {
    Context.prototype.initialize.call(this);
    let options = validateOptions({ selector: selector }, {
      selector: {
        is: ["string"],
        msg: "selector must be a string."
      }
    });
    internal(this).selector = options.selector;
  },

  serialize: function() {
    return {
      id: internal(this).id,
      type: "SelectorContext",
      args: [internal(this).selector]
    }
  }
});
exports.SelectorContext = SelectorContext;

// Matches when the page url matches any of the patterns given
let URLContext = Class({
  extends: Context,

  initialize: function initialize(patterns) {
    Context.prototype.initialize.call(this);
    patterns = Array.isArray(patterns) ? patterns : [patterns];

    try {
      internal(this).patterns = patterns.map(function (p) new MatchPattern(p));
    }
    catch (err) {
      throw new Error("Patterns must be a string, regexp or an array of " +
                      "strings or regexps: " + err);
    }
  },

  isCurrent: function isCurrent(url) {
    return internal(this).patterns.some(function (p) p.test(url));
  },

  serialize: function() {
    return {
      id: internal(this).id,
      type: "URLContext",
      args: []
    }
  }
});
exports.URLContext = URLContext;

// Matches when the user-supplied predicate returns true
let PredicateContext = Class({
  extends: Context,

  initialize: function initialize(predicate) {
    Context.prototype.initialize.call(this);
    let options = validateOptions({ predicate: predicate }, {
      predicate: {
        is: ["function"],
        msg: "predicate must be a function."
      }
    });
    internal(this).predicate = options.predicate;
  },

  isCurrent: function isCurrent(state) {
    return internal(this).predicate(state);
  },

  serialize: function() {
    return {
      id: internal(this).id,
      type: "PredicateContext",
      args: []
    }
  }
});
exports.PredicateContext = PredicateContext;

function removeItemFromArray(array, item) {
  return array.filter(function(i) i !== item);
}

// Converts anything that isn't false, null or undefined into a string
function stringOrNull(val) val ? String(val) : val;

// Shared option validation rules for Item, Menu, and Separator
let baseItemRules = {
  parentMenu: {
    is: ["object", "undefined"],
    ok: function (v) {
      if (!v)
        return true;
      return (v instanceof ItemContainer) || (v instanceof Menu);
    },
    msg: "parentMenu must be a Menu or not specified."
  },
  context: {
    is: ["undefined", "object", "array"],
    ok: function (v) {
      if (!v)
        return true;
      let arr = Array.isArray(v) ? v : [v];
      return arr.every(function (o) o instanceof Context);
    },
    msg: "The 'context' option must be a Context object or an array of " +
         "Context objects."
  },
  onMessage: {
    is: ["function", "undefined"]
  },
  contentScript: loaderContract.rules.contentScript,
  contentScriptFile: loaderContract.rules.contentScriptFile
};

let labelledItemRules =  mix(baseItemRules, {
  label: {
    map: stringOrNull,
    is: ["string"],
    ok: function (v) !!v,
    msg: "The item must have a non-empty string label."
  },
  accesskey: {
    map: stringOrNull,
    is: ["string", "undefined", "null"],
    ok: (v) => {
      if (!v) {
        return true;
      }
      return typeof v == "string" && v.length === 1;
    },
    msg: "The item must have a single character accesskey, or no accesskey."
  },
  image: {
    map: stringOrNull,
    is: ["string", "undefined", "null"],
    ok: function (url) {
      if (!url)
        return true;
      return isValidURI(url);
    },
    msg: "Image URL validation failed"
  }
});

// Additional validation rules for Item
let itemRules = mix(labelledItemRules, {
  data: {
    map: stringOrNull,
    is: ["string", "undefined", "null"]
  }
});

// Additional validation rules for Menu
let menuRules = mix(labelledItemRules, {
  items: {
    is: ["array", "undefined"],
    ok: function (v) {
      if (!v)
        return true;
      return v.every(function (item) {
        return item instanceof BaseItem;
      });
    },
    msg: "items must be an array, and each element in the array must be an " +
         "Item, Menu, or Separator."
  }
});

// Returns true if any contexts match. If there are no contexts then a
// PageContext is tested instead
function hasMatchingContext(contexts, addonInfo) {
  for (let context of contexts) {
    if (!(internal(context).id in addonInfo.contextStates)) {
      console.error("Missing state for context " + internal(context).id + " this is an error in the SDK modules.");
      return false;
    }
    if (!context.isCurrent(addonInfo.contextStates[internal(context).id]))
      return false;
  }

  return true;
}

// Tests whether an item should be visible or not based on its contexts and
// content scripts
function isItemVisible(item, addonInfo, usePageWorker) {
  if (!item.context.length) {
    if (!addonInfo.hasWorker)
      return usePageWorker ? addonInfo.pageContext : true;
  }

  if (!hasMatchingContext(item.context, addonInfo))
    return false;

  let context = addonInfo.workerContext;
  if (typeof(context) === "string" && context != "")
    item.label = context;

  return !!context;
}

// Called when an item is clicked to send out click events to the content
// scripts
function itemActivated(item, clickedNode) {
  let data = {
    items: [internal(item).id],
    data: item.data,
  }

  while (item.parentMenu) {
    item = item.parentMenu;
    data.items.push(internal(item).id);
  }

  let menuData = clickedNode.ownerDocument.defaultView.gContextMenuContentData;
  let messageManager = menuData.browser.messageManager;
  messageManager.sendAsyncMessage('sdk/contextmenu/activateitems', data, {
    popupNode: menuData.popupNode
  });
}

function serializeItem(item) {
  return {
    id: internal(item).id,
    contexts: [c.serialize() for (c of item.context)],
    contentScript: item.contentScript,
    contentScriptFile: item.contentScriptFile,
  };
}

// All things that appear in the context menu extend this
let BaseItem = Class({
  initialize: function initialize() {
    internal(this).id = uuid();

    internal(this).contexts = [];
    if ("context" in internal(this).options && internal(this).options.context) {
      let contexts = internal(this).options.context;
      if (Array.isArray(contexts)) {
        for (let context of contexts)
          internal(this).contexts.push(context);
      }
      else {
        internal(this).contexts.push(contexts);
      }
    }

    let parentMenu = internal(this).options.parentMenu;
    if (!parentMenu)
      parentMenu = contentContextMenu;

    parentMenu.addItem(this);

    Object.defineProperty(this, "contentScript", {
      enumerable: true,
      value: internal(this).options.contentScript
    });

    // Resolve URIs here as tests may have overriden self
    let files = internal(this).options.contentScriptFile;
    if (files) {
      if (!Array.isArray(files))
        files = [files];
      files = files.map(self.data.url);
    }
    internal(this).options.contentScriptFile = files;
    Object.defineProperty(this, "contentScriptFile", {
      enumerable: true,
      value: internal(this).options.contentScriptFile
    });

    // Notify all frames of this new item
    sendItems([serializeItem(this)]);
  },

  destroy: function destroy() {
    if (internal(this).destroyed)
      return;

    // Tell all existing frames that this item has been destroyed
    MessageManager.broadcastAsyncMessage("sdk/contextmenu/destroyitems", {
      items: [internal(this).id]
    });

    if (this.parentMenu)
      this.parentMenu.removeItem(this);

    internal(this).destroyed = true;
  },

  get context() {
    let contexts = internal(this).contexts.slice(0);
    contexts.add = (context) => {
      internal(this).contexts.push(context);
      // Notify all frames that this item has changed
      sendItems([serializeItem(this)]);
    };
    contexts.remove = (context) => {
      internal(this).contexts = internal(this).contexts.filter(c => {
        return c != context;
      });
      // Notify all frames that this item has changed
      sendItems([serializeItem(this)]);
    };
    return contexts;
  },

  set context(val) {
    internal(this).contexts = val.slice(0);
    // Notify all frames that this item has changed
    sendItems([serializeItem(this)]);
  },

  get parentMenu() {
    return internal(this).parentMenu;
  },
});

function workerMessageReceived({ data: { id, args } }) {
  if (internal(this).id != id)
    return;

  emit(this, ...args);
}

// All things that have a label on the context menu extend this
let LabelledItem = Class({
  extends: BaseItem,
  implements: [ EventTarget ],

  initialize: function initialize(options) {
    BaseItem.prototype.initialize.call(this);
    EventTarget.prototype.initialize.call(this, options);

    internal(this).messageListener = workerMessageReceived.bind(this);
    MessageManager.addMessageListener('sdk/worker/event', internal(this).messageListener);
  },

  destroy: function destroy() {
    if (internal(this).destroyed)
      return;

    MessageManager.removeMessageListener('sdk/worker/event', internal(this).messageListener);

    BaseItem.prototype.destroy.call(this);
  },

  get label() {
    return internal(this).options.label;
  },

  set label(val) {
    internal(this).options.label = val;

    MenuManager.updateItem(this);
  },

  get accesskey() {
    return internal(this).options.accesskey;
  },

  set accesskey(val) {
    internal(this).options.accesskey = val;

    MenuManager.updateItem(this);
  },

  get image() {
    return internal(this).options.image;
  },

  set image(val) {
    internal(this).options.image = val;

    MenuManager.updateItem(this);
  },

  get data() {
    return internal(this).options.data;
  },

  set data(val) {
    internal(this).options.data = val;
  }
});

let Item = Class({
  extends: LabelledItem,

  initialize: function initialize(options) {
    internal(this).options = validateOptions(options, itemRules);

    LabelledItem.prototype.initialize.call(this, options);
  },

  toString: function toString() {
    return "[object Item \"" + this.label + "\"]";
  },

  get data() {
    return internal(this).options.data;
  },

  set data(val) {
    internal(this).options.data = val;

    MenuManager.updateItem(this);
  },
});
exports.Item = Item;

let ItemContainer = Class({
  initialize: function initialize() {
    internal(this).children = [];
  },

  destroy: function destroy() {
    // Destroys the entire hierarchy
    for (let item of internal(this).children)
      item.destroy();
  },

  addItem: function addItem(item) {
    let oldParent = item.parentMenu;

    // Don't just call removeItem here as that would remove the corresponding
    // UI element which is more costly than just moving it to the right place
    if (oldParent)
      internal(oldParent).children = removeItemFromArray(internal(oldParent).children, item);

    let after = null;
    let children = internal(this).children;
    if (children.length > 0)
      after = children[children.length - 1];

    children.push(item);
    internal(item).parentMenu = this;

    // If there was an old parent then we just have to move the item, otherwise
    // it needs to be created
    if (oldParent)
      MenuManager.moveItem(item, after);
    else
      MenuManager.createItem(item, after);
  },

  removeItem: function removeItem(item) {
    // If the item isn't a child of this menu then ignore this call
    if (item.parentMenu !== this)
      return;

    MenuManager.removeItem(item);

    internal(this).children = removeItemFromArray(internal(this).children, item);
    internal(item).parentMenu = null;
  },

  get items() {
    return internal(this).children.slice(0);
  },

  set items(val) {
    // Validate the arguments before making any changes
    if (!Array.isArray(val))
      throw new Error(menuOptionRules.items.msg);

    for (let item of val) {
      if (!(item instanceof BaseItem))
        throw new Error(menuOptionRules.items.msg);
    }

    // Remove the old items and add the new ones
    for (let item of internal(this).children)
      this.removeItem(item);

    for (let item of val)
      this.addItem(item);
  },
});

let Menu = Class({
  extends: LabelledItem,
  implements: [ItemContainer],

  initialize: function initialize(options) {
    internal(this).options = validateOptions(options, menuRules);

    LabelledItem.prototype.initialize.call(this, options);
    ItemContainer.prototype.initialize.call(this);

    if (internal(this).options.items) {
      for (let item of internal(this).options.items)
        this.addItem(item);
    }
  },

  destroy: function destroy() {
    ItemContainer.prototype.destroy.call(this);
    LabelledItem.prototype.destroy.call(this);
  },

  toString: function toString() {
    return "[object Menu \"" + this.label + "\"]";
  },
});
exports.Menu = Menu;

let Separator = Class({
  extends: BaseItem,

  initialize: function initialize(options) {
    internal(this).options = validateOptions(options, baseItemRules);

    BaseItem.prototype.initialize.call(this);
  },

  toString: function toString() {
    return "[object Separator]";
  }
});
exports.Separator = Separator;

// Holds items for the content area context menu
let contentContextMenu = ItemContainer();
exports.contentContextMenu = contentContextMenu;

function getContainerItems(container) {
  let items = [];
  for (let item of internal(container).children) {
    items.push(serializeItem(item));
    if (item instanceof Menu)
      items = items.concat(getContainerItems(item));
  }
  return items;
}

// Notify all frames of these new or changed items
function sendItems(items) {
  MessageManager.broadcastAsyncMessage("sdk/contextmenu/createitems", {
    items,
    addon: ADDON,
  });
}

// Called when a new frame is created and wants to get the current list of items
function remoteItemRequest({ target: { messageManager } }) {
  let items = getContainerItems(contentContextMenu);
  if (items.length == 0)
    return;

  messageManager.sendAsyncMessage("sdk/contextmenu/createitems", {
    items,
    addon: ADDON,
  });
}
MessageManager.addMessageListener('sdk/contextmenu/requestitems', remoteItemRequest);

when(function() {
  MessageManager.removeMessageListener('sdk/contextmenu/requestitems', remoteItemRequest);
  contentContextMenu.destroy();
});

// App specific UI code lives here, it should handle populating the context
// menu and passing clicks etc. through to the items.

function countVisibleItems(nodes) {
  return Array.reduce(nodes, function(sum, node) {
    return node.hidden ? sum : sum + 1;
  }, 0);
}

let MenuWrapper = Class({
  initialize: function initialize(winWrapper, items, contextMenu) {
    this.winWrapper = winWrapper;
    this.window = winWrapper.window;
    this.items = items;
    this.contextMenu = contextMenu;
    this.populated = false;
    this.menuMap = new Map();

    // updateItemVisibilities will run first, updateOverflowState will run after
    // all other instances of this module have run updateItemVisibilities
    this._updateItemVisibilities = this.updateItemVisibilities.bind(this);
    this.contextMenu.addEventListener("popupshowing", this._updateItemVisibilities, true);
    this._updateOverflowState = this.updateOverflowState.bind(this);
    this.contextMenu.addEventListener("popupshowing", this._updateOverflowState, false);
  },

  destroy: function destroy() {
    this.contextMenu.removeEventListener("popupshowing", this._updateOverflowState, false);
    this.contextMenu.removeEventListener("popupshowing", this._updateItemVisibilities, true);

    if (!this.populated)
      return;

    // If we're getting unloaded at runtime then we must remove all the
    // generated XUL nodes
    let oldParent = null;
    for (let item of internal(this.items).children) {
      let xulNode = this.getXULNodeForItem(item);
      oldParent = xulNode.parentNode;
      oldParent.removeChild(xulNode);
    }

    if (oldParent)
      this.onXULRemoved(oldParent);
  },

  get separator() {
    return this.contextMenu.querySelector("." + SEPARATOR_CLASS);
  },

  get overflowMenu() {
    return this.contextMenu.querySelector("." + OVERFLOW_MENU_CLASS);
  },

  get overflowPopup() {
    return this.contextMenu.querySelector("." + OVERFLOW_POPUP_CLASS);
  },

  get topLevelItems() {
    return this.contextMenu.querySelectorAll("." + TOPLEVEL_ITEM_CLASS);
  },

  get overflowItems() {
    return this.contextMenu.querySelectorAll("." + OVERFLOW_ITEM_CLASS);
  },

  getXULNodeForItem: function getXULNodeForItem(item) {
    return this.menuMap.get(item);
  },

  // Recurses through the item hierarchy creating XUL nodes for everything
  populate: function populate(menu) {
    for (let i = 0; i < internal(menu).children.length; i++) {
      let item = internal(menu).children[i];
      let after = i === 0 ? null : internal(menu).children[i - 1];
      this.createItem(item, after);

      if (item instanceof Menu)
        this.populate(item);
    }
  },

  // Recurses through the menu setting the visibility of items. Returns true
  // if any of the items in this menu were visible
  setVisibility: function setVisibility(menu, addonInfo, usePageWorker) {
    let anyVisible = false;

    for (let item of internal(menu).children) {
      let visible = isItemVisible(item, addonInfo[internal(item).id], usePageWorker);

      // Recurse through Menus, if none of the sub-items were visible then the
      // menu is hidden too.
      if (visible && (item instanceof Menu))
        visible = this.setVisibility(item, addonInfo, false);

      let xulNode = this.getXULNodeForItem(item);
      xulNode.hidden = !visible;

      anyVisible = anyVisible || visible;
    }

    return anyVisible;
  },

  // Works out where to insert a XUL node for an item in a browser window
  insertIntoXUL: function insertIntoXUL(item, node, after) {
    let menupopup = null;
    let before = null;

    let menu = item.parentMenu;
    if (menu === this.items) {
      // Insert into the overflow popup if it exists, otherwise the normal
      // context menu
      menupopup = this.overflowPopup;
      if (!menupopup)
        menupopup = this.contextMenu;
    }
    else {
      let xulNode = this.getXULNodeForItem(menu);
      menupopup = xulNode.firstChild;
    }

    if (after) {
      let afterNode = this.getXULNodeForItem(after);
      before = afterNode.nextSibling;
    }
    else if (menupopup === this.contextMenu) {
      let topLevel = this.topLevelItems;
      if (topLevel.length > 0)
        before = topLevel[topLevel.length - 1].nextSibling;
      else
        before = this.separator.nextSibling;
    }

    menupopup.insertBefore(node, before);
  },

  // Sets the right class for XUL nodes
  updateXULClass: function updateXULClass(xulNode) {
    if (xulNode.parentNode == this.contextMenu)
      xulNode.classList.add(TOPLEVEL_ITEM_CLASS);
    else
      xulNode.classList.remove(TOPLEVEL_ITEM_CLASS);

    if (xulNode.parentNode == this.overflowPopup)
      xulNode.classList.add(OVERFLOW_ITEM_CLASS);
    else
      xulNode.classList.remove(OVERFLOW_ITEM_CLASS);
  },

  // Creates a XUL node for an item
  createItem: function createItem(item, after) {
    if (!this.populated)
      return;

    // Create the separator if it doesn't already exist
    if (!this.separator) {
      let separator = this.window.document.createElement("menuseparator");
      separator.setAttribute("class", SEPARATOR_CLASS);

      // Insert before the separator created by the old context-menu if it
      // exists to avoid bug 832401
      let oldSeparator = this.window.document.getElementById("jetpack-context-menu-separator");
      if (oldSeparator && oldSeparator.parentNode != this.contextMenu)
        oldSeparator = null;
      this.contextMenu.insertBefore(separator, oldSeparator);
    }

    let type = "menuitem";
    if (item instanceof Menu)
      type = "menu";
    else if (item instanceof Separator)
      type = "menuseparator";

    let xulNode = this.window.document.createElement(type);
    xulNode.setAttribute("class", ITEM_CLASS);
    if (item instanceof LabelledItem) {
      xulNode.setAttribute("label", item.label);
      if (item.accesskey)
        xulNode.setAttribute("accesskey", item.accesskey);
      if (item.image) {
        xulNode.setAttribute("image", item.image);
        if (item instanceof Menu)
          xulNode.classList.add("menu-iconic");
        else
          xulNode.classList.add("menuitem-iconic");
      }
      if (item.data)
        xulNode.setAttribute("value", item.data);

      let self = this;
      xulNode.addEventListener("command", function(event) {
        // Only care about clicks directly on this item
        if (event.target !== xulNode)
          return;

        itemActivated(item, xulNode);
      }, false);
    }

    this.insertIntoXUL(item, xulNode, after);
    this.updateXULClass(xulNode);
    xulNode.data = item.data;

    if (item instanceof Menu) {
      let menupopup = this.window.document.createElement("menupopup");
      xulNode.appendChild(menupopup);
    }

    this.menuMap.set(item, xulNode);
  },

  // Updates the XUL node for an item in this window
  updateItem: function updateItem(item) {
    if (!this.populated)
      return;

    let xulNode = this.getXULNodeForItem(item);

    // TODO figure out why this requires setAttribute
    xulNode.setAttribute("label", item.label);
    xulNode.setAttribute("accesskey", item.accesskey || "");

    if (item.image) {
      xulNode.setAttribute("image", item.image);
      if (item instanceof Menu)
        xulNode.classList.add("menu-iconic");
      else
        xulNode.classList.add("menuitem-iconic");
    }
    else {
      xulNode.removeAttribute("image");
      xulNode.classList.remove("menu-iconic");
      xulNode.classList.remove("menuitem-iconic");
    }

    if (item.data)
      xulNode.setAttribute("value", item.data);
    else
      xulNode.removeAttribute("value");
  },

  // Moves the XUL node for an item in this window to its new place in the
  // hierarchy
  moveItem: function moveItem(item, after) {
    if (!this.populated)
      return;

    let xulNode = this.getXULNodeForItem(item);
    let oldParent = xulNode.parentNode;

    this.insertIntoXUL(item, xulNode, after);
    this.updateXULClass(xulNode);
    this.onXULRemoved(oldParent);
  },

  // Removes the XUL nodes for an item in every window we've ever populated.
  removeItem: function removeItem(item) {
    if (!this.populated)
      return;

    let xulItem = this.getXULNodeForItem(item);

    let oldParent = xulItem.parentNode;

    oldParent.removeChild(xulItem);
    this.menuMap.delete(item);

    this.onXULRemoved(oldParent);
  },

  // Called when any XUL nodes have been removed from a menupopup. This handles
  // making sure the separator and overflow are correct
  onXULRemoved: function onXULRemoved(parent) {
    if (parent == this.contextMenu) {
      let toplevel = this.topLevelItems;

      // If there are no more items then remove the separator
      if (toplevel.length == 0) {
        let separator = this.separator;
        if (separator)
          separator.parentNode.removeChild(separator);
      }
    }
    else if (parent == this.overflowPopup) {
      // If there are no more items then remove the overflow menu and separator
      if (parent.childNodes.length == 0) {
        let separator = this.separator;
        separator.parentNode.removeChild(separator);
        this.contextMenu.removeChild(parent.parentNode);
      }
    }
  },

  // Recurses through all the items owned by this module and sets their hidden
  // state
  updateItemVisibilities: function updateItemVisibilities(event) {
    try {
      if (event.type != "popupshowing")
        return;
      if (event.target != this.contextMenu)
        return;

      if (internal(this.items).children.length == 0)
        return;

      if (!this.populated) {
        this.populated = true;
        this.populate(this.items);
      }

      let mainWindow = event.target.ownerDocument.defaultView;
      this.contextMenuContentData = mainWindow.gContextMenuContentData
      let addonInfo = this.contextMenuContentData.addonInfo[self.id];
      if (!addonInfo) {
        console.warn("No context menu state data was provided.");
        return;
      }
      this.setVisibility(this.items, addonInfo, true);
    }
    catch (e) {
      console.exception(e);
    }
  },

  // Counts the number of visible items across all modules and makes sure they
  // are in the right place between the top level context menu and the overflow
  // menu
  updateOverflowState: function updateOverflowState(event) {
    try {
      if (event.type != "popupshowing")
        return;
      if (event.target != this.contextMenu)
        return;

      // The main items will be in either the top level context menu or the
      // overflow menu at this point. Count the visible ones and if they are in
      // the wrong place move them
      let toplevel = this.topLevelItems;
      let overflow = this.overflowItems;
      let visibleCount = countVisibleItems(toplevel) +
                         countVisibleItems(overflow);

      if (visibleCount == 0) {
        let separator = this.separator;
        if (separator)
          separator.hidden = true;
        let overflowMenu = this.overflowMenu;
        if (overflowMenu)
          overflowMenu.hidden = true;
      }
      else if (visibleCount > MenuManager.overflowThreshold) {
        this.separator.hidden = false;
        let overflowPopup = this.overflowPopup;
        if (overflowPopup)
          overflowPopup.parentNode.hidden = false;

        if (toplevel.length > 0) {
          // The overflow menu shouldn't exist here but let's play it safe
          if (!overflowPopup) {
            let overflowMenu = this.window.document.createElement("menu");
            overflowMenu.setAttribute("class", OVERFLOW_MENU_CLASS);
            overflowMenu.setAttribute("label", OVERFLOW_MENU_LABEL);
            overflowMenu.setAttribute("accesskey", OVERFLOW_MENU_ACCESSKEY);
            this.contextMenu.insertBefore(overflowMenu, this.separator.nextSibling);

            overflowPopup = this.window.document.createElement("menupopup");
            overflowPopup.setAttribute("class", OVERFLOW_POPUP_CLASS);
            overflowMenu.appendChild(overflowPopup);
          }

          for (let xulNode of toplevel) {
            overflowPopup.appendChild(xulNode);
            this.updateXULClass(xulNode);
          }
        }
      }
      else {
        this.separator.hidden = false;

        if (overflow.length > 0) {
          // Move all the overflow nodes out of the overflow menu and position
          // them immediately before it
          for (let xulNode of overflow) {
            this.contextMenu.insertBefore(xulNode, xulNode.parentNode.parentNode);
            this.updateXULClass(xulNode);
          }
          this.contextMenu.removeChild(this.overflowMenu);
        }
      }
    }
    catch (e) {
      console.exception(e);
    }
  }
});

// This wraps every window that we've seen
let WindowWrapper = Class({
  initialize: function initialize(window) {
    this.window = window;
    this.menus = [
      new MenuWrapper(this, contentContextMenu, window.document.getElementById("contentAreaContextMenu")),
    ];
  },

  destroy: function destroy() {
    for (let menuWrapper of this.menus)
      menuWrapper.destroy();
  },

  getMenuWrapperForItem: function getMenuWrapperForItem(item) {
    let root = item.parentMenu;
    while (root.parentMenu)
      root = root.parentMenu;

    for (let wrapper of this.menus) {
      if (wrapper.items === root)
        return wrapper;
    }

    return null;
  }
});

let MenuManager = {
  windowMap: new Map(),

  get overflowThreshold() {
    let prefs = require("./preferences/service");
    return prefs.get(OVERFLOW_THRESH_PREF, OVERFLOW_THRESH_DEFAULT);
  },

  // When a new window is added start watching it for context menu shows
  onTrack: function onTrack(window) {
    if (!isBrowser(window))
      return;

    // Generally shouldn't happen, but just in case
    if (this.windowMap.has(window)) {
      console.warn("Already seen this window");
      return;
    }

    let winWrapper = WindowWrapper(window);
    this.windowMap.set(window, winWrapper);
  },

  onUntrack: function onUntrack(window) {
    if (!isBrowser(window))
      return;

    let winWrapper = this.windowMap.get(window);
    // This shouldn't happen but protect against it anyway
    if (!winWrapper)
      return;
    winWrapper.destroy();

    this.windowMap.delete(window);
  },

  // Creates a XUL node for an item in every window we've already populated
  createItem: function createItem(item, after) {
    for (let [window, winWrapper] of this.windowMap) {
      let menuWrapper = winWrapper.getMenuWrapperForItem(item);
      if (menuWrapper)
        menuWrapper.createItem(item, after);
    }
  },

  // Updates the XUL node for an item in every window we've already populated
  updateItem: function updateItem(item) {
    for (let [window, winWrapper] of this.windowMap) {
      let menuWrapper = winWrapper.getMenuWrapperForItem(item);
      if (menuWrapper)
        menuWrapper.updateItem(item);
    }
  },

  // Moves the XUL node for an item in every window we've ever populated to its
  // new place in the hierarchy
  moveItem: function moveItem(item, after) {
    for (let [window, winWrapper] of this.windowMap) {
      let menuWrapper = winWrapper.getMenuWrapperForItem(item);
      if (menuWrapper)
        menuWrapper.moveItem(item, after);
    }
  },

  // Removes the XUL nodes for an item in every window we've ever populated.
  removeItem: function removeItem(item) {
    for (let [window, winWrapper] of this.windowMap) {
      let menuWrapper = winWrapper.getMenuWrapperForItem(item);
      if (menuWrapper)
        menuWrapper.removeItem(item);
    }
  }
};

WindowTracker(MenuManager);
