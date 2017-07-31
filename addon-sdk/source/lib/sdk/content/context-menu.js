/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Class } = require("../core/heritage");
const self = require("../self");
const { WorkerChild } = require("./worker-child");
const { getInnerId } = require("../window/utils");
const { Ci } = require("chrome");
const { Services } = require("resource://gre/modules/Services.jsm");
const system = require('../system/events');
const { process } = require('../remote/child');

// These functions are roughly copied from sdk/selection which doesn't work
// in the content process
function getElementWithSelection(window) {
  let element = Services.focus.getFocusedElementForWindow(window, false, {});
  if (!element)
    return null;

  try {
    // Accessing selectionStart and selectionEnd on e.g. a button
    // results in an exception thrown as per the HTML5 spec.  See
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/association-of-controls-and-forms.html#textFieldSelection

    let { value, selectionStart, selectionEnd } = element;

    let hasSelection = typeof value === "string" &&
                      !isNaN(selectionStart) &&
                      !isNaN(selectionEnd) &&
                      selectionStart !== selectionEnd;

    return hasSelection ? element : null;
  }
  catch (err) {
    console.exception(err);
    return null;
  }
}

function safeGetRange(selection, rangeNumber) {
  try {
    let { rangeCount } = selection;
    let range = null;

    for (let rangeNumber = 0; rangeNumber < rangeCount; rangeNumber++ ) {
      range = selection.getRangeAt(rangeNumber);

      if (range && range.toString())
        break;

      range = null;
    }

    return range;
  }
  catch (e) {
    return null;
  }
}

function getSelection(window) {
  let selection = window.getSelection();
  let range = safeGetRange(selection);
  if (range)
    return range.toString();

  let node = getElementWithSelection(window);
  if (!node)
    return null;

  return node.value.substring(node.selectionStart, node.selectionEnd);
}

//These are used by PageContext.isCurrent below. If the popupNode or any of
//its ancestors is one of these, Firefox uses a tailored context menu, and so
//the page context doesn't apply.
const NON_PAGE_CONTEXT_ELTS = [
  Ci.nsIDOMHTMLAnchorElement,
  Ci.nsIDOMHTMLAreaElement,
  Ci.nsIDOMHTMLButtonElement,
  Ci.nsIDOMHTMLCanvasElement,
  Ci.nsIDOMHTMLEmbedElement,
  Ci.nsIDOMHTMLImageElement,
  Ci.nsIDOMHTMLInputElement,
  Ci.nsIDOMHTMLMapElement,
  Ci.nsIDOMHTMLMediaElement,
  Ci.nsIDOMHTMLMenuElement,
  Ci.nsIDOMHTMLObjectElement,
  Ci.nsIDOMHTMLOptionElement,
  Ci.nsIDOMHTMLSelectElement,
  Ci.nsIDOMHTMLTextAreaElement,
];

// List all editable types of inputs.  Or is it better to have a list
// of non-editable inputs?
var editableInputs = {
  email: true,
  number: true,
  password: true,
  search: true,
  tel: true,
  text: true,
  textarea: true,
  url: true
};

var CONTEXTS = {};

var Context = Class({
  initialize: function(id) {
    this.id = id;
  },

  adjustPopupNode: function adjustPopupNode(popupNode) {
    return popupNode;
  },

  // Gets state to pass through to the parent process for the node the user
  // clicked on
  getState: function(popupNode) {
    return false;
  }
});

// Matches when the context-clicked node doesn't have any of
// NON_PAGE_CONTEXT_ELTS in its ancestors
CONTEXTS.PageContext = Class({
  extends: Context,

  getState: function(popupNode) {
    // If there is a selection in the window then this context does not match
    if (!popupNode.ownerGlobal.getSelection().isCollapsed)
      return false;

    // If the clicked node or any of its ancestors is one of the blocked
    // NON_PAGE_CONTEXT_ELTS then this context does not match
    while (!(popupNode instanceof Ci.nsIDOMDocument)) {
      if (NON_PAGE_CONTEXT_ELTS.some(type => popupNode instanceof type))
        return false;

      popupNode = popupNode.parentNode;
    }

    return true;
  }
});

// Matches when there is an active selection in the window
CONTEXTS.SelectionContext = Class({
  extends: Context,

  getState: function(popupNode) {
    if (!popupNode.ownerGlobal.getSelection().isCollapsed)
      return true;

    try {
      // The node may be a text box which has selectionStart and selectionEnd
      // properties. If not this will throw.
      let { selectionStart, selectionEnd } = popupNode;
      return !isNaN(selectionStart) && !isNaN(selectionEnd) &&
             selectionStart !== selectionEnd;
    }
    catch (e) {
      return false;
    }
  }
});

// Matches when the context-clicked node or any of its ancestors matches the
// selector given
CONTEXTS.SelectorContext = Class({
  extends: Context,

  initialize: function initialize(id, selector) {
    Context.prototype.initialize.call(this, id);
    this.selector = selector;
  },

  adjustPopupNode: function adjustPopupNode(popupNode) {
    let selector = this.selector;

    while (!(popupNode instanceof Ci.nsIDOMDocument)) {
      if (popupNode.matches(selector))
        return popupNode;

      popupNode = popupNode.parentNode;
    }

    return null;
  },

  getState: function(popupNode) {
    return !!this.adjustPopupNode(popupNode);
  }
});

// Matches when the page url matches any of the patterns given
CONTEXTS.URLContext = Class({
  extends: Context,

  getState: function(popupNode) {
    return popupNode.ownerDocument.URL;
  }
});

// Matches when the user-supplied predicate returns true
CONTEXTS.PredicateContext = Class({
  extends: Context,

  getState: function(node) {
    let window = node.ownerGlobal;
    let data = {};

    data.documentType = node.ownerDocument.contentType;

    data.documentURL = node.ownerDocument.location.href;
    data.targetName = node.nodeName.toLowerCase();
    data.targetID = node.id || null ;

    if ((data.targetName === 'input' && editableInputs[node.type]) ||
        data.targetName === 'textarea') {
      data.isEditable = !node.readOnly && !node.disabled;
    }
    else {
      data.isEditable = node.isContentEditable;
    }

    data.selectionText = getSelection(window, "TEXT");

    data.srcURL = node.src || null;
    data.value = node.value || null;

    while (!data.linkURL && node) {
      data.linkURL = node.href || null;
      node = node.parentNode;
    }

    return data;
  },
});

function instantiateContext({ id, type, args }) {
  if (!(type in CONTEXTS)) {
    console.error("Attempt to use unknown context " + type);
    return;
  }
  return new CONTEXTS[type](id, ...args);
}

var ContextWorker = Class({
  implements: [ WorkerChild ],

  // Calls the context workers context listeners and returns the first result
  // that is either a string or a value that evaluates to true. If all of the
  // listeners returned false then returns false. If there are no listeners,
  // returns true (show the menu item by default).
  getMatchedContext: function getCurrentContexts(popupNode) {
    let results = this.sandbox.emitSync("context", popupNode);
    if (!results.length)
      return true;
    return results.reduce((val, result) => val || result);
  },

  // Emits a click event in the worker's port. popupNode is the node that was
  // context-clicked, and clickedItemData is the data of the item that was
  // clicked.
  fireClick: function fireClick(popupNode, clickedItemData) {
    this.sandbox.emitSync("click", popupNode, clickedItemData);
  }
});

// Gets the item's content script worker for a window, creating one if necessary
// Once created it will be automatically destroyed when the window unloads.
// If there is not content scripts for the item then null will be returned.
function getItemWorkerForWindow(item, window) {
  if (!item.contentScript && !item.contentScriptFile)
    return null;

  let id = getInnerId(window);
  let worker = item.workerMap.get(id);

  if (worker)
    return worker;

  worker = ContextWorker({
    id: item.id,
    window,
    manager: item.manager,
    contentScript: item.contentScript,
    contentScriptFile: item.contentScriptFile,
    onDetach: function() {
      item.workerMap.delete(id);
    }
  });

  item.workerMap.set(id, worker);

  return worker;
}

// A very simple remote proxy for every item. It's job is to provide data for
// the main process to use to determine visibility state and to call into
// content scripts when clicked.
var RemoteItem = Class({
  initialize: function(options, manager) {
    this.id = options.id;
    this.contexts = options.contexts.map(instantiateContext);
    this.contentScript = options.contentScript;
    this.contentScriptFile = options.contentScriptFile;

    this.manager = manager;

    this.workerMap = new Map();
    keepAlive.set(this.id, this);
  },

  destroy: function() {
    for (let worker of this.workerMap.values()) {
      worker.destroy();
    }
    keepAlive.delete(this.id);
  },

  activate: function(popupNode, data) {
    let worker = getItemWorkerForWindow(this, popupNode.ownerGlobal);
    if (!worker)
      return;

    for (let context of this.contexts)
      popupNode = context.adjustPopupNode(popupNode);

    worker.fireClick(popupNode, data);
  },

  // Fills addonInfo with state data to send through to the main process
  getContextState: function(popupNode, addonInfo) {
    if (!(self.id in addonInfo)) {
      addonInfo[self.id] = {
        processID: process.id,
        items: {}
      };
    }

    let worker = getItemWorkerForWindow(this, popupNode.ownerGlobal);
    let contextStates = {};
    for (let context of this.contexts)
      contextStates[context.id] = context.getState(popupNode);

    addonInfo[self.id].items[this.id] = {
      // It isn't ideal to create a PageContext for every item but there isn't
      // a good shared place to do it.
      pageContext: (new CONTEXTS.PageContext()).getState(popupNode),
      contextStates,
      hasWorker: !!worker,
      workerContext: worker ? worker.getMatchedContext(popupNode) : true
    }
  }
});
exports.RemoteItem = RemoteItem;

// Holds remote items for this frame.
var keepAlive = new Map();

// Called to create remote proxies for items. If they already exist we destroy
// and recreate. This can happen if the item changes in some way or in odd
// timing cases where the frame script is create around the same time as the
// item is created in the main process
process.port.on('sdk/contextmenu/createitems', (process, items) => {
  for (let itemoptions of items) {
    let oldItem = keepAlive.get(itemoptions.id);
    if (oldItem) {
      oldItem.destroy();
    }

    let item = new RemoteItem(itemoptions, this);
  }
});

process.port.on('sdk/contextmenu/destroyitems', (process, items) => {
  for (let id of items) {
    let item = keepAlive.get(id);
    item.destroy();
  }
});

var lastPopupNode = null;

system.on('content-contextmenu', ({ subject }) => {
  let { event: { target: popupNode }, addonInfo } = subject.wrappedJSObject;
  lastPopupNode = popupNode;

  for (let item of keepAlive.values()) {
    item.getContextState(popupNode, addonInfo);
  }
}, true);

process.port.on('sdk/contextmenu/activateitems', (process, items, data) => {
  for (let id of items) {
    let item = keepAlive.get(id);
    if (!item)
      continue;

    item.activate(lastPopupNode, data);
  }
});
