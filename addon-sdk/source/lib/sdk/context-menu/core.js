/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Contexts = require("./context");
const Readers = require("./readers");
const Component = require("../ui/component");
const { Class } = require("../core/heritage");
const { map, filter, object, reduce, keys, symbols,
        pairs, values, each, some, isEvery, count } = require("../util/sequence");
const { loadModule } = require("framescript/manager");
const { Cu, Cc, Ci } = require("chrome");
const prefs = require("sdk/preferences/service");

const globalMessageManager = Cc["@mozilla.org/globalmessagemanager;1"]
                              .getService(Ci.nsIMessageListenerManager);
const preferencesService = Cc["@mozilla.org/preferences-service;1"].
                            getService(Ci.nsIPrefService).
                            getBranch(null);


const readTable = Symbol("context-menu/read-table");
const nameTable = Symbol("context-menu/name-table");
const onContext = Symbol("context-menu/on-context");
const isMatching = Symbol("context-menu/matching-handler?");

exports.onContext = onContext;
exports.readTable = readTable;
exports.nameTable = nameTable;


const propagateOnContext = (item, data) =>
  each(child => child[onContext](data), item.state.children);

const isContextMatch = item => !item[isMatching] || item[isMatching]();

// For whatever reason addWeakMessageListener does not seems to work as our
// instance seems to dropped even though it's alive. This is simple workaround
// to avoid dead object excetptions.
const WeakMessageListener = function(receiver, handler="receiveMessage") {
  this.receiver = receiver
  this.handler = handler
};
WeakMessageListener.prototype = {
  constructor: WeakMessageListener,
  receiveMessage(message) {
    if (Cu.isDeadWrapper(this.receiver)) {
      message.target.messageManager.removeMessageListener(message.name, this);
    }
    else {
      this.receiver[this.handler](message);
    }
  }
};

const OVERFLOW_THRESH = "extensions.addon-sdk.context-menu.overflowThreshold";
const onMessage = Symbol("context-menu/message-listener");
const onPreferceChange = Symbol("context-menu/preference-change");
const ContextMenuExtension = Class({
  extends: Component,
  initialize: Component,
  setup() {
    const messageListener = new WeakMessageListener(this, onMessage);
    loadModule(globalMessageManager, "framescript/context-menu", true, "onContentFrame");
    globalMessageManager.addMessageListener("sdk/context-menu/read", messageListener);
    globalMessageManager.addMessageListener("sdk/context-menu/readers?", messageListener);

    preferencesService.addObserver(OVERFLOW_THRESH, this);
  },
  observe(_, __, name) {
    if (name === OVERFLOW_THRESH) {
      const overflowThreshold = prefs.get(OVERFLOW_THRESH, 10);
      this[Component.patch]({overflowThreshold});
    }
  },
  [onMessage]({name, data, target}) {
    if (name === "sdk/context-menu/read")
      this[onContext]({target, data});
    if (name === "sdk/context-menu/readers?")
      target.messageManager.sendAsyncMessage("sdk/context-menu/readers",
                                             JSON.parse(JSON.stringify(this.state.readers)));
  },
  [Component.initial](options={}, children) {
    const element = options.element || null;
    const target = options.target || null;
    const readers = Object.create(null);
    const users = Object.create(null);
    const registry = new WeakSet();
    const overflowThreshold = prefs.get(OVERFLOW_THRESH, 10);

    return { target, children: [], readers, users, element,
             registry, overflowThreshold };
  },
  [Component.isUpdated](before, after) {
    // Update only if target changed, since there is no point in re-rendering
    // when children are. Also new items added won't be in sync with a latest
    // context target so we should really just render before drawing context
    // menu.
    return before.target !== after.target;
  },
  [Component.render]({element, children, overflowThreshold}) {
    if (!element) return null;

    const items = children.filter(isContextMatch);
    const body = items.length === 0 ? items :
                 items.length < overflowThreshold ? [new Separator(),
                                                     ...items] :
                 [{tagName: "menu",
                   className: "sdk-context-menu-overflow-menu",
                   label: "Add-ons",
                   accesskey: "A",
                   children: [{tagName: "menupopup",
                               children: items}]}];
    return {
      element: element,
      tagName: "menugroup",
      style: "-moz-box-orient: vertical;",
      className: "sdk-context-menu-extension",
      children: body
    }
  },
  // Adds / remove child to it's own list.
  add(item) {
    this[Component.patch]({children: this.state.children.concat(item)});
  },
  remove(item) {
    this[Component.patch]({
      children: this.state.children.filter(x => x !== item)
    });
  },
  register(item) {
    const { users, registry } = this.state;
    if (registry.has(item)) return;
    registry.add(item);

    // Each (ContextHandler) item has a readTable that is a
    // map of keys to readers extracting them from the content.
    // During the registraction we update intrnal record of unique
    // readers and users per reader. Most context will have a reader
    // shared across all instances there for map of users per reader
    // is stored separately from the reader so that removing reader
    // will occur only when no users remain.
    const table = item[readTable];
    // Context readers store data in private symbols so we need to
    // collect both table keys and private symbols.
    const names = [...keys(table), ...symbols(table)];
    const readers = map(name => table[name], names);
    // Create delta for registered readers that will be merged into
    // internal readers table.
    const added = filter(x => !users[x.id], readers);
    const delta = object(...map(x => [x.id, x], added));

    const update = reduce((update, reader) => {
      const n = update[reader.id] || 0;
      update[reader.id] = n + 1;
      return update;
    }, Object.assign({}, users), readers);

    // Patch current state with a changes that registered item caused.
    this[Component.patch]({users: update,
                           readers: Object.assign(this.state.readers, delta)});

    if (count(added)) {
      globalMessageManager.broadcastAsyncMessage("sdk/context-menu/readers",
                                                 JSON.parse(JSON.stringify(delta)));
    }
  },
  unregister(item) {
    const { users, registry } = this.state;
    if (!registry.has(item)) return;
    registry.delete(item);

    const table = item[readTable];
    const names = [...keys(table), ...symbols(table)];
    const readers = map(name => table[name], names);
    const update = reduce((update, reader) => {
      update[reader.id] = update[reader.id] - 1;
      return update;
    }, Object.assign({}, users), readers);
    const removed = filter(id => !update[id], keys(update));
    const delta = object(...map(x => [x, null], removed));

    this[Component.patch]({users: update,
                           readers: Object.assign(this.state.readers, delta)});

    if (count(removed)) {
      globalMessageManager.broadcastAsyncMessage("sdk/context-menu/readers",
                                                 JSON.parse(JSON.stringify(delta)));
    }
  },

  [onContext]({data, target}) {
    propagateOnContext(this, data);
    const document = target.ownerDocument;
    const element = document.getElementById("contentAreaContextMenu");

    this[Component.patch]({target: data, element: element});
  }
});this,
exports.ContextMenuExtension = ContextMenuExtension;

// Takes an item options and
const makeReadTable = ({context, read}) => {
  // Result of this function is a tuple of all readers &
  // name, reader id pairs.

  // Filter down to contexts that have a reader associated.
  const contexts = filter(context => context.read, context);
  // Merge all contexts read maps to a single hash, note that there should be
  // no name collisions as context implementations expect to use private
  // symbols for storing it's read data.
  return Object.assign({}, ...map(({read}) => read, contexts), read);
}

const readTarget = (nameTable, data) =>
  object(...map(([name, id]) => [name, data[id]], nameTable))

const ContextHandler = Class({
  extends: Component,
  initialize: Component,
  get context() {
    return this.state.options.context;
  },
  get read() {
    return this.state.options.read;
  },
  [Component.initial](options) {
    return {
      table: makeReadTable(options),
      requiredContext: filter(context => context.isRequired, options.context),
      optionalContext: filter(context => !context.isRequired, options.context)
    }
  },
  [isMatching]() {
    const {target, requiredContext, optionalContext} = this.state;
    return isEvery(context => context.isCurrent(target), requiredContext) &&
            (count(optionalContext) === 0 ||
             some(context => context.isCurrent(target), optionalContext));
  },
  setup() {
    const table = makeReadTable(this.state.options);
    this[readTable] = table;
    this[nameTable] = [...map(symbol => [symbol, table[symbol].id], symbols(table)),
                       ...map(name => [name, table[name].id], keys(table))];


    contextMenu.register(this);

    each(child => contextMenu.remove(child), this.state.children);
    contextMenu.add(this);
  },
  dispose() {
    contextMenu.remove(this);

    each(child => contextMenu.unregister(child), this.state.children);
    contextMenu.unregister(this);
  },
  // Internal `Symbol("onContext")` method is invoked when "contextmenu" event
  // occurs in content process. Context handles with children delegate to each
  // child and patch it's internal state to reflect new contextmenu target.
  [onContext](data) {
    propagateOnContext(this, data);
    this[Component.patch]({target: readTarget(this[nameTable], data)});
  }
});
const isContextHandler = item => item instanceof ContextHandler;

exports.ContextHandler = ContextHandler;

const Menu = Class({
  extends: ContextHandler,
  [isMatching]() {
    return ContextHandler.prototype[isMatching].call(this) &&
           this.state.children.filter(isContextHandler)
                              .some(isContextMatch);
  },
  [Component.render]({children, options}) {
    const items = children.filter(isContextMatch);
    return {tagName: "menu",
            className: "sdk-context-menu menu-iconic",
            label: options.label,
            accesskey: options.accesskey,
            image: options.icon,
            children: [{tagName: "menupopup",
                        children: items}]};
  }
});
exports.Menu = Menu;

const onCommand = Symbol("context-menu/item/onCommand");
const Item = Class({
  extends: ContextHandler,
  get onClick() {
    return this.state.options.onClick;
  },
  [Component.render]({options}) {
    const {label, icon, accesskey} = options;
    return {tagName: "menuitem",
            className: "sdk-context-menu-item menuitem-iconic",
            label,
            accesskey,
            image: icon,
            oncommand: this};
  },
  handleEvent(event) {
    if (this.onClick)
      this.onClick(this.state.target);
  }
});
exports.Item = Item;

var Separator = Class({
  extends: Component,
  initialize: Component,
  [Component.render]() {
    return {tagName: "menuseparator",
            className: "sdk-context-menu-separator"}
  },
  [onContext]() {

  }
});
exports.Separator = Separator;

exports.Contexts = Contexts;
exports.Readers = Readers;

const createElement = (vnode, {document}) => {
   const node = vnode.namespace ?
              document.createElementNS(vnode.namespace, vnode.tagName) :
              document.createElement(vnode.tagName);

   node.setAttribute("data-component-path", vnode[Component.path]);

   each(([key, value]) => {
     if (key === "tagName") {
       return;
     }
     if (key === "children") {
       return;
     }

     if (key.startsWith("on")) {
       node.addEventListener(key.substr(2), value)
       return;
     }

     if (typeof(value) !== "object" &&
         typeof(value) !== "function" &&
         value !== void(0) &&
         value !== null)
    {
       if (key === "className") {
         node[key] = value;
       }
       else {
         node.setAttribute(key, value);
       }
       return;
     }
   }, pairs(vnode));

  each(child => node.appendChild(createElement(child, {document})), vnode.children);
  return node;
};

const htmlWriter = tree => {
  if (tree !== null) {
    const root = tree.element;
    const node = createElement(tree, {document: root.ownerDocument});
    const before = root.querySelector("[data-component-path='/']");
    if (before) {
      root.replaceChild(node, before);
    } else {
      root.appendChild(node);
    }
  }
};


const contextMenu = ContextMenuExtension();
exports.contextMenu = contextMenu;
Component.mount(contextMenu, htmlWriter);
