/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Here's the server side of the remote inspector.
 *
 * The WalkerActor is the client's view of the debuggee's DOM.  It's gives
 * the client a tree of NodeActor objects.
 *
 * The walker presents the DOM tree mostly unmodified from the source DOM
 * tree, but with a few key differences:
 *
 *  - Empty text nodes are ignored.  This is pretty typical of developer
 *    tools, but maybe we should reconsider that on the server side.
 *  - iframes with documents loaded have the loaded document as the child,
 *    the walker provides one big tree for the whole document tree.
 *
 * There are a few ways to get references to NodeActors:
 *
 *   - When you first get a WalkerActor reference, it comes with a free
 *     reference to the root document's node.
 *   - Given a node, you can ask for children, siblings, and parents.
 *   - You can issue querySelector and querySelectorAll requests to find
 *     other elements.
 *   - Requests that return arbitrary nodes from the tree (like querySelector
 *     and querySelectorAll) will also return any nodes the client hasn't
 *     seen in order to have a complete set of parents.
 *
 * Once you have a NodeFront, you should be able to answer a few questions
 * without further round trips, like the node's name, namespace/tagName,
 * attributes, etc.  Other questions (like a text node's full nodeValue)
 * might require another round trip.
 *
 * The protocol guarantees that the client will always know the parent of
 * any node that is returned by the server.  This means that some requests
 * (like querySelector) will include the extra nodes needed to satisfy this
 * requirement.  The client keeps track of this parent relationship, so the
 * node fronts form a tree that is a subset of the actual DOM tree.
 *
 *
 * We maintain this guarantee to support the ability to release subtrees on
 * the client - when a node is disconnected from the DOM tree we want to be
 * able to free the client objects for all the children nodes.
 *
 * So to be able to answer "all the children of a given node that we have
 * seen on the client side", we guarantee that every time we've seen a node,
 * we connect it up through its parents.
 */

const {Cc, Ci, Cu} = require("chrome");
const Services = require("Services");
const protocol = require("devtools/shared/protocol");
const {LayoutActor} = require("devtools/server/actors/layout");
const {LongStringActor} = require("devtools/server/actors/string");
const promise = require("promise");
const {Task} = require("devtools/shared/task");
const events = require("sdk/event/core");
const {WalkerSearch} = require("devtools/server/actors/utils/walker-search");
const {PageStyleActor, getFontPreviewData} = require("devtools/server/actors/styles");
const {
  HighlighterActor,
  CustomHighlighterActor,
  isTypeRegistered,
  HighlighterEnvironment
} = require("devtools/server/actors/highlighters");
const {EyeDropper} = require("devtools/server/actors/highlighters/eye-dropper");
const {
  isAnonymous,
  isNativeAnonymous,
  isXBLAnonymous,
  isShadowAnonymous,
  getFrameElement
} = require("devtools/shared/layout/utils");
const {getLayoutChangesObserver, releaseLayoutChangesObserver} = require("devtools/server/actors/reflow");
const nodeFilterConstants = require("devtools/shared/dom-node-filter-constants");

const {EventParsers} = require("devtools/server/event-parsers");
const {nodeSpec, nodeListSpec, walkerSpec, inspectorSpec} = require("devtools/shared/specs/inspector");

const FONT_FAMILY_PREVIEW_TEXT = "The quick brown fox jumps over the lazy dog";
const FONT_FAMILY_PREVIEW_TEXT_SIZE = 20;
const PSEUDO_CLASSES = [":hover", ":active", ":focus"];
const HIDDEN_CLASS = "__fx-devtools-hide-shortcut__";
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const IMAGE_FETCHING_TIMEOUT = 500;
const RX_FUNC_NAME =
  /((var|const|let)\s+)?([\w$.]+\s*[:=]\s*)*(function)?\s*\*?\s*([\w$]+)?\s*$/;

// The possible completions to a ':' with added score to give certain values
// some preference.
const PSEUDO_SELECTORS = [
  [":active", 1],
  [":hover", 1],
  [":focus", 1],
  [":visited", 0],
  [":link", 0],
  [":first-letter", 0],
  [":first-child", 2],
  [":before", 2],
  [":after", 2],
  [":lang(", 0],
  [":not(", 3],
  [":first-of-type", 0],
  [":last-of-type", 0],
  [":only-of-type", 0],
  [":only-child", 2],
  [":nth-child(", 3],
  [":nth-last-child(", 0],
  [":nth-of-type(", 0],
  [":nth-last-of-type(", 0],
  [":last-child", 2],
  [":root", 0],
  [":empty", 0],
  [":target", 0],
  [":enabled", 0],
  [":disabled", 0],
  [":checked", 1],
  ["::selection", 0]
];

var HELPER_SHEET = `
  .__fx-devtools-hide-shortcut__ {
    visibility: hidden !important;
  }

  :-moz-devtools-highlighted {
    outline: 2px dashed #F06!important;
    outline-offset: -2px !important;
  }
`;

const flags = require("devtools/shared/flags");

loader.lazyRequireGetter(this, "DevToolsUtils",
                         "devtools/shared/DevToolsUtils");

loader.lazyRequireGetter(this, "AsyncUtils", "devtools/shared/async-utils");

loader.lazyGetter(this, "DOMParser", function () {
  return Cc["@mozilla.org/xmlextras/domparser;1"]
           .createInstance(Ci.nsIDOMParser);
});

loader.lazyGetter(this, "eventListenerService", function () {
  return Cc["@mozilla.org/eventlistenerservice;1"]
           .getService(Ci.nsIEventListenerService);
});

loader.lazyRequireGetter(this, "CssLogic", "devtools/server/css-logic", true);
loader.lazyRequireGetter(this, "findCssSelector", "devtools/shared/inspector/css-logic", true);

/**
 * We only send nodeValue up to a certain size by default.  This stuff
 * controls that size.
 */
exports.DEFAULT_VALUE_SUMMARY_LENGTH = 50;
var gValueSummaryLength = exports.DEFAULT_VALUE_SUMMARY_LENGTH;

exports.getValueSummaryLength = function () {
  return gValueSummaryLength;
};

exports.setValueSummaryLength = function (val) {
  gValueSummaryLength = val;
};

/**
 * Returns the properly cased version of the node's tag name, which can be
 * used when displaying said name in the UI.
 *
 * @param  {Node} rawNode
 *         Node for which we want the display name
 * @return {String}
 *         Properly cased version of the node tag name
 */
const getNodeDisplayName = function (rawNode) {
  if (rawNode.nodeName && !rawNode.localName) {
    // The localName & prefix APIs have been moved from the Node interface to the Element
    // interface. Use Node.nodeName as a fallback.
    return rawNode.nodeName;
  }
  return (rawNode.prefix ? rawNode.prefix + ":" : "") + rawNode.localName;
};
exports.getNodeDisplayName = getNodeDisplayName;

/**
 * Server side of the node actor.
 */
var NodeActor = exports.NodeActor = protocol.ActorClassWithSpec(nodeSpec, {
  initialize: function (walker, node) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.walker = walker;
    this.rawNode = node;
    this._eventParsers = new EventParsers().parsers;

    // Storing the original display of the node, to track changes when reflows
    // occur
    this.wasDisplayed = this.isDisplayed;
  },

  toString: function () {
    return "[NodeActor " + this.actorID + " for " +
      this.rawNode.toString() + "]";
  },

  /**
   * Instead of storing a connection object, the NodeActor gets its connection
   * from its associated walker.
   */
  get conn() {
    return this.walker.conn;
  },

  isDocumentElement: function () {
    return this.rawNode.ownerDocument &&
           this.rawNode.ownerDocument.documentElement === this.rawNode;
  },

  destroy: function () {
    protocol.Actor.prototype.destroy.call(this);

    if (this.mutationObserver) {
      if (!Cu.isDeadWrapper(this.mutationObserver)) {
        this.mutationObserver.disconnect();
      }
      this.mutationObserver = null;
    }
    this.rawNode = null;
    this.walker = null;
  },

  // Returns the JSON representation of this object over the wire.
  form: function (detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let parentNode = this.walker.parentNode(this);
    let inlineTextChild = this.walker.inlineTextChild(this);

    let form = {
      actor: this.actorID,
      baseURI: this.rawNode.baseURI,
      parent: parentNode ? parentNode.actorID : undefined,
      nodeType: this.rawNode.nodeType,
      namespaceURI: this.rawNode.namespaceURI,
      nodeName: this.rawNode.nodeName,
      nodeValue: this.rawNode.nodeValue,
      displayName: getNodeDisplayName(this.rawNode),
      numChildren: this.numChildren,
      inlineTextChild: inlineTextChild ? inlineTextChild.form() : undefined,

      // doctype attributes
      name: this.rawNode.name,
      publicId: this.rawNode.publicId,
      systemId: this.rawNode.systemId,

      attrs: this.writeAttrs(),
      isBeforePseudoElement: this.isBeforePseudoElement,
      isAfterPseudoElement: this.isAfterPseudoElement,
      isAnonymous: isAnonymous(this.rawNode),
      isNativeAnonymous: isNativeAnonymous(this.rawNode),
      isXBLAnonymous: isXBLAnonymous(this.rawNode),
      isShadowAnonymous: isShadowAnonymous(this.rawNode),
      pseudoClassLocks: this.writePseudoClassLocks(),

      isDisplayed: this.isDisplayed,
      isInHTMLDocument: this.rawNode.ownerDocument &&
        this.rawNode.ownerDocument.contentType === "text/html",
      hasEventListeners: this._hasEventListeners,
    };

    if (this.isDocumentElement()) {
      form.isDocumentElement = true;
    }

    // Add an extra API for custom properties added by other
    // modules/extensions.
    form.setFormProperty = (name, value) => {
      if (!form.props) {
        form.props = {};
      }
      form.props[name] = value;
    };

    // Fire an event so, other modules can create its own properties
    // that should be passed to the client (within the form.props field).
    events.emit(NodeActor, "form", {
      target: this,
      data: form
    });

    return form;
  },

  /**
   * Watch the given document node for mutations using the DOM observer
   * API.
   */
  watchDocument: function (callback) {
    let node = this.rawNode;
    // Create the observer on the node's actor.  The node will make sure
    // the observer is cleaned up when the actor is released.
    let observer = new node.defaultView.MutationObserver(callback);
    observer.mergeAttributeRecords = true;
    observer.observe(node, {
      nativeAnonymousChildList: true,
      attributes: true,
      characterData: true,
      characterDataOldValue: true,
      childList: true,
      subtree: true
    });
    this.mutationObserver = observer;
  },

  get isBeforePseudoElement() {
    return this.rawNode.nodeName === "_moz_generated_content_before";
  },

  get isAfterPseudoElement() {
    return this.rawNode.nodeName === "_moz_generated_content_after";
  },

  // Estimate the number of children that the walker will return without making
  // a call to children() if possible.
  get numChildren() {
    // For pseudo elements, childNodes.length returns 1, but the walker
    // will return 0.
    if (this.isBeforePseudoElement || this.isAfterPseudoElement) {
      return 0;
    }

    let rawNode = this.rawNode;
    let numChildren = rawNode.childNodes.length;
    let hasAnonChildren = rawNode.nodeType === Ci.nsIDOMNode.ELEMENT_NODE &&
                          rawNode.ownerDocument.getAnonymousNodes(rawNode);

    let hasContentDocument = rawNode.contentDocument;
    let hasSVGDocument = rawNode.getSVGDocument && rawNode.getSVGDocument();
    if (numChildren === 0 && (hasContentDocument || hasSVGDocument)) {
      // This might be an iframe with virtual children.
      numChildren = 1;
    }

    // Normal counting misses ::before/::after.  Also, some anonymous children
    // may ultimately be skipped, so we have to consult with the walker.
    if (numChildren === 0 || hasAnonChildren) {
      numChildren = this.walker.children(this).nodes.length;
    }

    return numChildren;
  },

  get computedStyle() {
    return CssLogic.getComputedStyle(this.rawNode);
  },

  /**
   * Is the node's display computed style value other than "none"
   */
  get isDisplayed() {
    // Consider all non-element nodes as displayed.
    if (isNodeDead(this) ||
        this.rawNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE ||
        this.isAfterPseudoElement ||
        this.isBeforePseudoElement) {
      return true;
    }

    let style = this.computedStyle;
    if (!style) {
      return true;
    }

    return style.display !== "none";
  },

  /**
   * Are there event listeners that are listening on this node? This method
   * uses all parsers registered via event-parsers.js.registerEventParser() to
   * check if there are any event listeners.
   */
  get _hasEventListeners() {
    let parsers = this._eventParsers;
    for (let [, {hasListeners}] of parsers) {
      try {
        if (hasListeners && hasListeners(this.rawNode)) {
          return true;
        }
      } catch (e) {
        // An object attached to the node looked like a listener but wasn't...
        // do nothing.
      }
    }
    return false;
  },

  writeAttrs: function () {
    if (!this.rawNode.attributes) {
      return undefined;
    }

    return [...this.rawNode.attributes].map(attr => {
      return {namespace: attr.namespace, name: attr.name, value: attr.value };
    });
  },

  writePseudoClassLocks: function () {
    if (this.rawNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE) {
      return undefined;
    }
    let ret = undefined;
    for (let pseudo of PSEUDO_CLASSES) {
      if (DOMUtils.hasPseudoClassLock(this.rawNode, pseudo)) {
        ret = ret || [];
        ret.push(pseudo);
      }
    }
    return ret;
  },

  /**
   * Gets event listeners and adds their information to the events array.
   *
   * @param  {Node} node
   *         Node for which we are to get listeners.
   */
  getEventListeners: function (node) {
    let parsers = this._eventParsers;
    let dbg = this.parent().tabActor.makeDebugger();
    let listeners = [];

    for (let [, {getListeners, normalizeHandler}] of parsers) {
      try {
        let eventInfos = getListeners(node);

        if (!eventInfos) {
          continue;
        }

        for (let eventInfo of eventInfos) {
          if (normalizeHandler) {
            eventInfo.normalizeHandler = normalizeHandler;
          }

          this.processHandlerForEvent(node, listeners, dbg, eventInfo);
        }
      } catch (e) {
        // An object attached to the node looked like a listener but wasn't...
        // do nothing.
      }
    }

    listeners.sort((a, b) => {
      return a.type.localeCompare(b.type);
    });

    return listeners;
  },

  /**
   * Process a handler
   *
   * @param  {Node} node
   *         The node for which we want information.
   * @param  {Array} events
   *         The events array contains all event objects that we have gathered
   *         so far.
   * @param  {Debugger} dbg
   *         JSDebugger instance.
   * @param  {Object} eventInfo
   *         See event-parsers.js.registerEventParser() for a description of the
   *         eventInfo object.
   *
   * @return {Array}
   *         An array of objects where a typical object looks like this:
   *           {
   *             type: "click",
   *             handler: function() { doSomething() },
   *             origin: "http://www.mozilla.com",
   *             searchString: 'onclick="doSomething()"',
   *             tags: tags,
   *             DOM0: true,
   *             capturing: true,
   *             hide: {
   *               dom0: true
   *             }
   *           }
   */
  processHandlerForEvent: function (node, listeners, dbg, eventInfo) {
    let type = eventInfo.type || "";
    let handler = eventInfo.handler;
    let tags = eventInfo.tags || "";
    let hide = eventInfo.hide || {};
    let override = eventInfo.override || {};
    let global = Cu.getGlobalForObject(handler);
    let globalDO = dbg.addDebuggee(global);
    let listenerDO = globalDO.makeDebuggeeValue(handler);

    if (eventInfo.normalizeHandler) {
      listenerDO = eventInfo.normalizeHandler(listenerDO);
    }

    // If the listener is an object with a 'handleEvent' method, use that.
    if (listenerDO.class === "Object" || listenerDO.class === "XULElement") {
      let desc;

      while (!desc && listenerDO) {
        desc = listenerDO.getOwnPropertyDescriptor("handleEvent");
        listenerDO = listenerDO.proto;
      }

      if (desc && desc.value) {
        listenerDO = desc.value;
      }
    }

    if (listenerDO.isBoundFunction) {
      listenerDO = listenerDO.boundTargetFunction;
    }

    let script = listenerDO.script;
    let scriptSource = script.source.text;
    let functionSource =
      scriptSource.substr(script.sourceStart, script.sourceLength);

    /*
    The script returned is the whole script and
    scriptSource.substr(script.sourceStart, script.sourceLength) returns
    something like this:
      () { doSomething(); }

    So we need to use some regex magic to get the appropriate function info
    e.g.:
      () => { ... }
      function doit() { ... }
      doit: function() { ... }
      es6func() { ... }
      var|let|const foo = function () { ... }
      function generator*() { ... }
    */
    let scriptBeforeFunc = scriptSource.substr(0, script.sourceStart);
    let matches = scriptBeforeFunc.match(RX_FUNC_NAME);
    if (matches && matches.length > 0) {
      functionSource = matches[0].trim() + functionSource;
    }

    let dom0 = false;

    if (typeof node.hasAttribute !== "undefined") {
      dom0 = !!node.hasAttribute("on" + type);
    } else {
      dom0 = !!node["on" + type];
    }

    let line = script.startLine;
    let url = script.url;
    let origin = url + (dom0 ? "" : ":" + line);
    let searchString;

    if (dom0) {
      searchString = "on" + type + "=\"" + script.source.text + "\"";
    } else {
      scriptSource = "    " + scriptSource;
    }

    let eventObj = {
      type: typeof override.type !== "undefined" ? override.type : type,
      handler: functionSource.trim(),
      origin: typeof override.origin !== "undefined" ?
                     override.origin : origin,
      searchString: typeof override.searchString !== "undefined" ?
                           override.searchString : searchString,
      tags: tags,
      DOM0: typeof override.dom0 !== "undefined" ? override.dom0 : dom0,
      capturing: typeof override.capturing !== "undefined" ?
                        override.capturing : eventInfo.capturing,
      hide: hide
    };

    listeners.push(eventObj);

    dbg.removeDebuggee(globalDO);
  },

  /**
   * Returns a LongStringActor with the node's value.
   */
  getNodeValue: function () {
    return new LongStringActor(this.conn, this.rawNode.nodeValue || "");
  },

  /**
   * Set the node's value to a given string.
   */
  setNodeValue: function (value) {
    this.rawNode.nodeValue = value;
  },

  /**
   * Get a unique selector string for this node.
   */
  getUniqueSelector: function () {
    if (Cu.isDeadWrapper(this.rawNode)) {
      return "";
    }
    return findCssSelector(this.rawNode);
  },

  /**
   * Scroll the selected node into view.
   */
  scrollIntoView: function () {
    this.rawNode.scrollIntoView(true);
  },

  /**
   * Get the node's image data if any (for canvas and img nodes).
   * Returns an imageData object with the actual data being a LongStringActor
   * and a size json object.
   * The image data is transmitted as a base64 encoded png data-uri.
   * The method rejects if the node isn't an image or if the image is missing
   *
   * Accepts a maxDim request parameter to resize images that are larger. This
   * is important as the resizing occurs server-side so that image-data being
   * transfered in the longstring back to the client will be that much smaller
   */
  getImageData: function (maxDim) {
    return imageToImageData(this.rawNode, maxDim).then(imageData => {
      return {
        data: LongStringActor(this.conn, imageData.data),
        size: imageData.size
      };
    });
  },

  /**
   * Get all event listeners that are listening on this node.
   */
  getEventListenerInfo: function () {
    if (this.rawNode.nodeName.toLowerCase() === "html") {
      return this.getEventListeners(this.rawNode.ownerGlobal);
    }
    return this.getEventListeners(this.rawNode);
  },

  /**
   * Modify a node's attributes.  Passed an array of modifications
   * similar in format to "attributes" mutations.
   * {
   *   attributeName: <string>
   *   attributeNamespace: <optional string>
   *   newValue: <optional string> - If null or undefined, the attribute
   *     will be removed.
   * }
   *
   * Returns when the modifications have been made.  Mutations will
   * be queued for any changes made.
   */
  modifyAttributes: function (modifications) {
    let rawNode = this.rawNode;
    for (let change of modifications) {
      if (change.newValue == null) {
        if (change.attributeNamespace) {
          rawNode.removeAttributeNS(change.attributeNamespace,
                                    change.attributeName);
        } else {
          rawNode.removeAttribute(change.attributeName);
        }
      } else if (change.attributeNamespace) {
        rawNode.setAttributeNS(change.attributeNamespace, change.attributeName,
                               change.newValue);
      } else {
        rawNode.setAttribute(change.attributeName, change.newValue);
      }
    }
  },

  /**
   * Given the font and fill style, get the image data of a canvas with the
   * preview text and font.
   * Returns an imageData object with the actual data being a LongStringActor
   * and the width of the text as a string.
   * The image data is transmitted as a base64 encoded png data-uri.
   */
  getFontFamilyDataURL: function (font, fillStyle = "black") {
    let doc = this.rawNode.ownerDocument;
    let options = {
      previewText: FONT_FAMILY_PREVIEW_TEXT,
      previewFontSize: FONT_FAMILY_PREVIEW_TEXT_SIZE,
      fillStyle: fillStyle
    };
    let { dataURL, size } = getFontPreviewData(font, doc, options);

    return { data: LongStringActor(this.conn, dataURL), size: size };
  }
});

/**
 * Server side of a node list as returned by querySelectorAll()
 */
var NodeListActor = exports.NodeListActor = protocol.ActorClassWithSpec(nodeListSpec, {
  typeName: "domnodelist",

  initialize: function (walker, nodeList) {
    protocol.Actor.prototype.initialize.call(this);
    this.walker = walker;
    this.nodeList = nodeList || [];
  },

  destroy: function () {
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Instead of storing a connection object, the NodeActor gets its connection
   * from its associated walker.
   */
  get conn() {
    return this.walker.conn;
  },

  /**
   * Items returned by this actor should belong to the parent walker.
   */
  marshallPool: function () {
    return this.walker;
  },

  // Returns the JSON representation of this object over the wire.
  form: function () {
    return {
      actor: this.actorID,
      length: this.nodeList ? this.nodeList.length : 0
    };
  },

  /**
   * Get a single node from the node list.
   */
  item: function (index) {
    return this.walker.attachElement(this.nodeList[index]);
  },

  /**
   * Get a range of the items from the node list.
   */
  items: function (start = 0, end = this.nodeList.length) {
    let items = Array.prototype.slice.call(this.nodeList, start, end)
      .map(item => this.walker._ref(item));
    return this.walker.attachElements(items);
  },

  release: function () {}
});

/**
 * Server side of the DOM walker.
 */
var WalkerActor = protocol.ActorClassWithSpec(walkerSpec, {
  /**
   * Create the WalkerActor
   * @param DebuggerServerConnection conn
   *    The server connection.
   */
  initialize: function (conn, tabActor, options) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this.rootWin = tabActor.window;
    this.rootDoc = this.rootWin.document;
    this._refMap = new Map();
    this._pendingMutations = [];
    this._activePseudoClassLocks = new Set();
    this.showAllAnonymousContent = options.showAllAnonymousContent;

    this.walkerSearch = new WalkerSearch(this);

    // Nodes which have been removed from the client's known
    // ownership tree are considered "orphaned", and stored in
    // this set.
    this._orphaned = new Set();

    // The client can tell the walker that it is interested in a node
    // even when it is orphaned with the `retainNode` method.  This
    // list contains orphaned nodes that were so retained.
    this._retainedOrphans = new Set();

    this.onMutations = this.onMutations.bind(this);
    this.onFrameLoad = this.onFrameLoad.bind(this);
    this.onFrameUnload = this.onFrameUnload.bind(this);

    events.on(tabActor, "will-navigate", this.onFrameUnload);
    events.on(tabActor, "navigate", this.onFrameLoad);

    // Ensure that the root document node actor is ready and
    // managed.
    this.rootNode = this.document();

    this.layoutChangeObserver = getLayoutChangesObserver(this.tabActor);
    this._onReflows = this._onReflows.bind(this);
    this.layoutChangeObserver.on("reflows", this._onReflows);
    this._onResize = this._onResize.bind(this);
    this.layoutChangeObserver.on("resize", this._onResize);

    this._onEventListenerChange = this._onEventListenerChange.bind(this);
    eventListenerService.addListenerChangeListener(this._onEventListenerChange);
  },

  /**
   * Callback for eventListenerService.addListenerChangeListener
   * @param nsISimpleEnumerator changesEnum
   *    enumerator of nsIEventListenerChange
   */
  _onEventListenerChange: function (changesEnum) {
    let changes = changesEnum.enumerate();
    while (changes.hasMoreElements()) {
      let current = changes.getNext().QueryInterface(Ci.nsIEventListenerChange);
      let target = current.target;

      if (this._refMap.has(target)) {
        let actor = this.getNode(target);
        let mutation = {
          type: "events",
          target: actor.actorID,
          hasEventListeners: actor._hasEventListeners
        };
        this.queueMutation(mutation);
      }
    }
  },

  // Returns the JSON representation of this object over the wire.
  form: function () {
    return {
      actor: this.actorID,
      root: this.rootNode.form(),
      traits: {
        // FF42+ Inspector starts managing the Walker, while the inspector also
        // starts cleaning itself up automatically on client disconnection.
        // So that there is no need to manually release the walker anymore.
        autoReleased: true,
        // XXX: It seems silly that we need to tell the front which capabilities
        // its actor has in this way when the target can use actorHasMethod. If
        // this was ported to the protocol (Bug 1157048) we could call that
        // inside of custom front methods and not need to do traits for this.
        multiFrameQuerySelectorAll: true,
        textSearch: true,
      }
    };
  },

  toString: function () {
    return "[WalkerActor " + this.actorID + "]";
  },

  getDocumentWalker: function (node, whatToShow) {
    // Allow native anon content (like <video> controls) if preffed on
    let nodeFilter = this.showAllAnonymousContent
                        ? allAnonymousContentTreeWalkerFilter
                        : standardTreeWalkerFilter;
    return new DocumentWalker(node, this.rootWin, whatToShow, nodeFilter);
  },

  destroy: function () {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;
    protocol.Actor.prototype.destroy.call(this);
    try {
      this.clearPseudoClassLocks();
      this._activePseudoClassLocks = null;

      this._hoveredNode = null;
      this.rootWin = null;
      this.rootDoc = null;
      this.rootNode = null;
      this.layoutHelpers = null;
      this._orphaned = null;
      this._retainedOrphans = null;
      this._refMap = null;

      events.off(this.tabActor, "will-navigate", this.onFrameUnload);
      events.off(this.tabActor, "navigate", this.onFrameLoad);

      this.onFrameLoad = null;
      this.onFrameUnload = null;

      this.walkerSearch.destroy();

      this.layoutChangeObserver.off("reflows", this._onReflows);
      this.layoutChangeObserver.off("resize", this._onResize);
      this.layoutChangeObserver = null;
      releaseLayoutChangesObserver(this.tabActor);

      eventListenerService.removeListenerChangeListener(
        this._onEventListenerChange);

      this.onMutations = null;

      this.layoutActor = null;
      this.tabActor = null;

      events.emit(this, "destroyed");
    } catch (e) {
      console.error(e);
    }
  },

  release: function () {},

  unmanage: function (actor) {
    if (actor instanceof NodeActor) {
      if (this._activePseudoClassLocks &&
          this._activePseudoClassLocks.has(actor)) {
        this.clearPseudoClassLocks(actor);
      }
      this._refMap.delete(actor.rawNode);
    }
    protocol.Actor.prototype.unmanage.call(this, actor);
  },

  /**
   * Determine if the walker has come across this DOM node before.
   * @param {DOMNode} rawNode
   * @return {Boolean}
   */
  hasNode: function (rawNode) {
    return this._refMap.has(rawNode);
  },

  /**
   * If the walker has come across this DOM node before, then get the
   * corresponding node actor.
   * @param {DOMNode} rawNode
   * @return {NodeActor}
   */
  getNode: function (rawNode) {
    return this._refMap.get(rawNode);
  },

  _ref: function (node) {
    let actor = this.getNode(node);
    if (actor) {
      return actor;
    }

    actor = new NodeActor(this, node);

    // Add the node actor as a child of this walker actor, assigning
    // it an actorID.
    this.manage(actor);
    this._refMap.set(node, actor);

    if (node.nodeType === Ci.nsIDOMNode.DOCUMENT_NODE) {
      actor.watchDocument(this.onMutations);
    }
    return actor;
  },

  _onReflows: function (reflows) {
    // Going through the nodes the walker knows about, see which ones have
    // had their display changed and send a display-change event if any
    let changes = [];
    for (let [node, actor] of this._refMap) {
      if (Cu.isDeadWrapper(node)) {
        continue;
      }

      let isDisplayed = actor.isDisplayed;
      if (isDisplayed !== actor.wasDisplayed) {
        changes.push(actor);
        // Updating the original value
        actor.wasDisplayed = isDisplayed;
      }
    }

    if (changes.length) {
      events.emit(this, "display-change", changes);
    }
  },

  /**
   * When the browser window gets resized, relay the event to the front.
   */
  _onResize: function () {
    events.emit(this, "resize");
  },

  /**
   * This is kept for backward-compatibility reasons with older remote targets.
   * Targets prior to bug 916443.
   *
   * pick/cancelPick are used to pick a node on click on the content
   * document. But in their implementation prior to bug 916443, they don't allow
   * highlighting on hover.
   * The client-side now uses the highlighter actor's pick and cancelPick
   * methods instead. The client-side uses the the highlightable trait found in
   * the root actor to determine which version of pick to use.
   *
   * As for highlight, the new highlighter actor is used instead of the walker's
   * highlight method. Same here though, the client-side uses the highlightable
   * trait to dertermine which to use.
   *
   * Keeping these actor methods for now allows newer client-side debuggers to
   * inspect fxos 1.2 remote targets or older firefox desktop remote targets.
   */
  pick: function () {},
  cancelPick: function () {},
  highlight: function (node) {},

  /**
   * Ensures that the node is attached and it can be accessed from the root.
   *
   * @param {(Node|NodeActor)} nodes The nodes
   * @return {Object} An object compatible with the disconnectedNode type.
   */
  attachElement: function (node) {
    let { nodes, newParents } = this.attachElements([node]);
    return {
      node: nodes[0],
      newParents: newParents
    };
  },

  /**
   * Ensures that the nodes are attached and they can be accessed from the root.
   *
   * @param {(Node[]|NodeActor[])} nodes The nodes
   * @return {Object} An object compatible with the disconnectedNodeArray type.
   */
  attachElements: function (nodes) {
    let nodeActors = [];
    let newParents = new Set();
    for (let node of nodes) {
      if (!(node instanceof NodeActor)) {
        // If an anonymous node was passed in and we aren't supposed to know
        // about it, then consult with the document walker as the source of
        // truth about which elements exist.
        if (!this.showAllAnonymousContent && isAnonymous(node)) {
          node = this.getDocumentWalker(node).currentNode;
        }

        node = this._ref(node);
      }

      this.ensurePathToRoot(node, newParents);
      // If nodes may be an array of raw nodes, we're sure to only have
      // NodeActors with the following array.
      nodeActors.push(node);
    }

    return {
      nodes: nodeActors,
      newParents: [...newParents]
    };
  },

  /**
   * Return the document node that contains the given node,
   * or the root node if no node is specified.
   * @param NodeActor node
   *        The node whose document is needed, or null to
   *        return the root.
   */
  document: function (node) {
    let doc = isNodeDead(node) ? this.rootDoc : nodeDocument(node.rawNode);
    return this._ref(doc);
  },

  /**
   * Return the documentElement for the document containing the
   * given node.
   * @param NodeActor node
   *        The node whose documentElement is requested, or null
   *        to use the root document.
   */
  documentElement: function (node) {
    let elt = isNodeDead(node)
              ? this.rootDoc.documentElement
              : nodeDocument(node.rawNode).documentElement;
    return this._ref(elt);
  },

  /**
   * Return all parents of the given node, ordered from immediate parent
   * to root.
   * @param NodeActor node
   *    The node whose parents are requested.
   * @param object options
   *    Named options, including:
   *    `sameDocument`: If true, parents will be restricted to the same
   *      document as the node.
   *    `sameTypeRootTreeItem`: If true, this will not traverse across
   *     different types of docshells.
   */
  parents: function (node, options = {}) {
    if (isNodeDead(node)) {
      return [];
    }

    let walker = this.getDocumentWalker(node.rawNode);
    let parents = [];
    let cur;
    while ((cur = walker.parentNode())) {
      if (options.sameDocument &&
          nodeDocument(cur) != nodeDocument(node.rawNode)) {
        break;
      }

      if (options.sameTypeRootTreeItem &&
          nodeDocshell(cur).sameTypeRootTreeItem !=
          nodeDocshell(node.rawNode).sameTypeRootTreeItem) {
        break;
      }

      parents.push(this._ref(cur));
    }
    return parents;
  },

  parentNode: function (node) {
    let walker = this.getDocumentWalker(node.rawNode);
    let parent = walker.parentNode();
    if (parent) {
      return this._ref(parent);
    }
    return null;
  },

  /**
   * If the given NodeActor only has a single text node as a child with a text
   * content small enough to be inlined, return that child's NodeActor.
   *
   * @param NodeActor node
   */
  inlineTextChild: function (node) {
    // Quick checks to prevent creating a new walker if possible.
    if (node.isBeforePseudoElement ||
        node.isAfterPseudoElement ||
        node.rawNode.nodeType != Ci.nsIDOMNode.ELEMENT_NODE ||
        node.rawNode.children.length > 0) {
      return undefined;
    }

    let docWalker = this.getDocumentWalker(node.rawNode);
    let firstChild = docWalker.firstChild();

    // Bail out if:
    // - more than one child
    // - unique child is not a text node
    // - unique child is a text node, but is too long to be inlined
    if (!firstChild ||
        docWalker.nextSibling() ||
        firstChild.nodeType !== Ci.nsIDOMNode.TEXT_NODE ||
        firstChild.nodeValue.length > gValueSummaryLength
        ) {
      return undefined;
    }

    return this._ref(firstChild);
  },

  /**
   * Mark a node as 'retained'.
   *
   * A retained node is not released when `releaseNode` is called on its
   * parent, or when a parent is released with the `cleanup` option to
   * `getMutations`.
   *
   * When a retained node's parent is released, a retained mode is added to
   * the walker's "retained orphans" list.
   *
   * Retained nodes can be deleted by providing the `force` option to
   * `releaseNode`.  They will also be released when their document
   * has been destroyed.
   *
   * Retaining a node makes no promise about its children;  They can
   * still be removed by normal means.
   */
  retainNode: function (node) {
    node.retained = true;
  },

  /**
   * Remove the 'retained' mark from a node.  If the node was a
   * retained orphan, release it.
   */
  unretainNode: function (node) {
    node.retained = false;
    if (this._retainedOrphans.has(node)) {
      this._retainedOrphans.delete(node);
      this.releaseNode(node);
    }
  },

  /**
   * Release actors for a node and all child nodes.
   */
  releaseNode: function (node, options = {}) {
    if (isNodeDead(node)) {
      return;
    }

    if (node.retained && !options.force) {
      this._retainedOrphans.add(node);
      return;
    }

    if (node.retained) {
      // Forcing a retained node to go away.
      this._retainedOrphans.delete(node);
    }

    let walker = this.getDocumentWalker(node.rawNode);

    let child = walker.firstChild();
    while (child) {
      let childActor = this.getNode(child);
      if (childActor) {
        this.releaseNode(childActor, options);
      }
      child = walker.nextSibling();
    }

    node.destroy();
  },

  /**
   * Add any nodes between `node` and the walker's root node that have not
   * yet been seen by the client.
   */
  ensurePathToRoot: function (node, newParents = new Set()) {
    if (!node) {
      return newParents;
    }
    let walker = this.getDocumentWalker(node.rawNode);
    let cur;
    while ((cur = walker.parentNode())) {
      let parent = this.getNode(cur);
      if (!parent) {
        // This parent didn't exist, so hasn't been seen by the client yet.
        newParents.add(this._ref(cur));
      } else {
        // This parent did exist, so the client knows about it.
        return newParents;
      }
    }
    return newParents;
  },

  /**
   * Return children of the given node.  By default this method will return
   * all children of the node, but there are options that can restrict this
   * to a more manageable subset.
   *
   * @param NodeActor node
   *    The node whose children you're curious about.
   * @param object options
   *    Named options:
   *    `maxNodes`: The set of nodes returned by the method will be no longer
   *       than maxNodes.
   *    `start`: If a node is specified, the list of nodes will start
   *       with the given child.  Mutally exclusive with `center`.
   *    `center`: If a node is specified, the given node will be as centered
   *       as possible in the list, given how close to the ends of the child
   *       list it is.  Mutually exclusive with `start`.
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   *
   * @returns an object with three items:
   *    hasFirst: true if the first child of the node is included in the list.
   *    hasLast: true if the last child of the node is included in the list.
   *    nodes: Child nodes returned by the request.
   */
  children: function (node, options = {}) {
    if (isNodeDead(node)) {
      return { hasFirst: true, hasLast: true, nodes: [] };
    }

    if (options.center && options.start) {
      throw Error("Can't specify both 'center' and 'start' options.");
    }
    let maxNodes = options.maxNodes || -1;
    if (maxNodes == -1) {
      maxNodes = Number.MAX_VALUE;
    }

    // We're going to create a few document walkers with the same filter,
    // make it easier.
    let getFilteredWalker = documentWalkerNode => {
      return this.getDocumentWalker(documentWalkerNode, options.whatToShow);
    };

    // Need to know the first and last child.
    let rawNode = node.rawNode;
    let firstChild = getFilteredWalker(rawNode).firstChild();
    let lastChild = getFilteredWalker(rawNode).lastChild();

    if (!firstChild) {
      // No children, we're done.
      return { hasFirst: true, hasLast: true, nodes: [] };
    }

    let start;
    if (options.center) {
      start = options.center.rawNode;
    } else if (options.start) {
      start = options.start.rawNode;
    } else {
      start = firstChild;
    }

    let nodes = [];

    // Start by reading backward from the starting point if we're centering...
    let backwardWalker = getFilteredWalker(start);
    if (start != firstChild && options.center) {
      backwardWalker.previousSibling();
      let backwardCount = Math.floor(maxNodes / 2);
      let backwardNodes = this._readBackward(backwardWalker, backwardCount);
      nodes = backwardNodes;
    }

    // Then read forward by any slack left in the max children...
    let forwardWalker = getFilteredWalker(start);
    let forwardCount = maxNodes - nodes.length;
    nodes = nodes.concat(this._readForward(forwardWalker, forwardCount));

    // If there's any room left, it means we've run all the way to the end.
    // If we're centering, check if there are more items to read at the front.
    let remaining = maxNodes - nodes.length;
    if (options.center && remaining > 0 && nodes[0].rawNode != firstChild) {
      let firstNodes = this._readBackward(backwardWalker, remaining);

      // Then put it all back together.
      nodes = firstNodes.concat(nodes);
    }

    return {
      hasFirst: nodes[0].rawNode == firstChild,
      hasLast: nodes[nodes.length - 1].rawNode == lastChild,
      nodes: nodes
    };
  },

  /**
   * Return siblings of the given node.  By default this method will return
   * all siblings of the node, but there are options that can restrict this
   * to a more manageable subset.
   *
   * If `start` or `center` are not specified, this method will center on the
   * node whose siblings are requested.
   *
   * @param NodeActor node
   *    The node whose children you're curious about.
   * @param object options
   *    Named options:
   *    `maxNodes`: The set of nodes returned by the method will be no longer
   *       than maxNodes.
   *    `start`: If a node is specified, the list of nodes will start
   *       with the given child.  Mutally exclusive with `center`.
   *    `center`: If a node is specified, the given node will be as centered
   *       as possible in the list, given how close to the ends of the child
   *       list it is.  Mutually exclusive with `start`.
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   *
   * @returns an object with three items:
   *    hasFirst: true if the first child of the node is included in the list.
   *    hasLast: true if the last child of the node is included in the list.
   *    nodes: Child nodes returned by the request.
   */
  siblings: function (node, options = {}) {
    if (isNodeDead(node)) {
      return { hasFirst: true, hasLast: true, nodes: [] };
    }

    let parentNode = this.getDocumentWalker(node.rawNode, options.whatToShow)
                         .parentNode();
    if (!parentNode) {
      return {
        hasFirst: true,
        hasLast: true,
        nodes: [node]
      };
    }

    if (!(options.start || options.center)) {
      options.center = node;
    }

    return this.children(this._ref(parentNode), options);
  },

  /**
   * Get the next sibling of a given node.  Getting nodes one at a time
   * might be inefficient, be careful.
   *
   * @param object options
   *    Named options:
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   */
  nextSibling: function (node, options = {}) {
    if (isNodeDead(node)) {
      return null;
    }

    let walker = this.getDocumentWalker(node.rawNode, options.whatToShow);
    let sibling = walker.nextSibling();
    return sibling ? this._ref(sibling) : null;
  },

  /**
   * Get the previous sibling of a given node.  Getting nodes one at a time
   * might be inefficient, be careful.
   *
   * @param object options
   *    Named options:
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   */
  previousSibling: function (node, options = {}) {
    if (isNodeDead(node)) {
      return null;
    }

    let walker = this.getDocumentWalker(node.rawNode, options.whatToShow);
    let sibling = walker.previousSibling();
    return sibling ? this._ref(sibling) : null;
  },

  /**
   * Helper function for the `children` method: Read forward in the sibling
   * list into an array with `count` items, including the current node.
   */
  _readForward: function (walker, count) {
    let ret = [];
    let node = walker.currentNode;
    do {
      ret.push(this._ref(node));
      node = walker.nextSibling();
    } while (node && --count);
    return ret;
  },

  /**
   * Helper function for the `children` method: Read backward in the sibling
   * list into an array with `count` items, including the current node.
   */
  _readBackward: function (walker, count) {
    let ret = [];
    let node = walker.currentNode;
    do {
      ret.push(this._ref(node));
      node = walker.previousSibling();
    } while (node && --count);
    ret.reverse();
    return ret;
  },

  /**
   * Return the first node in the document that matches the given selector.
   * See https://developer.mozilla.org/en-US/docs/Web/API/Element.querySelector
   *
   * @param NodeActor baseNode
   * @param string selector
   */
  querySelector: function (baseNode, selector) {
    if (isNodeDead(baseNode)) {
      return {};
    }

    let node = baseNode.rawNode.querySelector(selector);
    if (!node) {
      return {};
    }

    return this.attachElement(node);
  },

  /**
   * Return a NodeListActor with all nodes that match the given selector.
   * See https://developer.mozilla.org/en-US/docs/Web/API/Element.querySelectorAll
   *
   * @param NodeActor baseNode
   * @param string selector
   */
  querySelectorAll: function (baseNode, selector) {
    let nodeList = null;

    try {
      nodeList = baseNode.rawNode.querySelectorAll(selector);
    } catch (e) {
      // Bad selector. Do nothing as the selector can come from a searchbox.
    }

    return new NodeListActor(this, nodeList);
  },

  /**
   * Get a list of nodes that match the given selector in all known frames of
   * the current content page.
   * @param {String} selector.
   * @return {Array}
   */
  _multiFrameQuerySelectorAll: function (selector) {
    let nodes = [];

    for (let {document} of this.tabActor.windows) {
      try {
        nodes = [...nodes, ...document.querySelectorAll(selector)];
      } catch (e) {
        // Bad selector. Do nothing as the selector can come from a searchbox.
      }
    }

    return nodes;
  },

  /**
   * Return a NodeListActor with all nodes that match the given selector in all
   * frames of the current content page.
   * @param {String} selector
   */
  multiFrameQuerySelectorAll: function (selector) {
    return new NodeListActor(this, this._multiFrameQuerySelectorAll(selector));
  },

  /**
   * Search the document for a given string.
   * Results will be searched with the walker-search module (searches through
   * tag names, attribute names and values, and text contents).
   *
   * @returns {searchresult}
   *            - {NodeList} list
   *            - {Array<Object>} metadata. Extra information with indices that
   *                              match up with node list.
   */
  search: function (query) {
    let results = this.walkerSearch.search(query);
    let nodeList = new NodeListActor(this, results.map(r => r.node));

    return {
      list: nodeList,
      metadata: []
    };
  },

  /**
   * Returns a list of matching results for CSS selector autocompletion.
   *
   * @param string query
   *        The selector query being completed
   * @param string completing
   *        The exact token being completed out of the query
   * @param string selectorState
   *        One of "pseudo", "id", "tag", "class", "null"
   */
  getSuggestionsForQuery: function (query, completing, selectorState) {
    let sugs = {
      classes: new Map(),
      tags: new Map(),
      ids: new Map()
    };
    let result = [];
    let nodes = null;
    // Filtering and sorting the results so that protocol transfer is miminal.
    switch (selectorState) {
      case "pseudo":
        result = PSEUDO_SELECTORS.filter(item => {
          return item[0].startsWith(":" + completing);
        });
        break;

      case "class":
        if (!query) {
          nodes = this._multiFrameQuerySelectorAll("[class]");
        } else {
          nodes = this._multiFrameQuerySelectorAll(query);
        }
        for (let node of nodes) {
          for (let className of node.classList) {
            sugs.classes.set(className, (sugs.classes.get(className)|0) + 1);
          }
        }
        sugs.classes.delete("");
        sugs.classes.delete(HIDDEN_CLASS);
        for (let [className, count] of sugs.classes) {
          if (className.startsWith(completing)) {
            result.push(["." + CSS.escape(className), count, selectorState]);
          }
        }
        break;

      case "id":
        if (!query) {
          nodes = this._multiFrameQuerySelectorAll("[id]");
        } else {
          nodes = this._multiFrameQuerySelectorAll(query);
        }
        for (let node of nodes) {
          sugs.ids.set(node.id, (sugs.ids.get(node.id)|0) + 1);
        }
        for (let [id, count] of sugs.ids) {
          if (id.startsWith(completing) && id !== "") {
            result.push(["#" + CSS.escape(id), count, selectorState]);
          }
        }
        break;

      case "tag":
        if (!query) {
          nodes = this._multiFrameQuerySelectorAll("*");
        } else {
          nodes = this._multiFrameQuerySelectorAll(query);
        }
        for (let node of nodes) {
          let tag = node.localName;
          sugs.tags.set(tag, (sugs.tags.get(tag)|0) + 1);
        }
        for (let [tag, count] of sugs.tags) {
          if ((new RegExp("^" + completing + ".*", "i")).test(tag)) {
            result.push([tag, count, selectorState]);
          }
        }

        // For state 'tag' (no preceding # or .) and when there's no query (i.e.
        // only one word) then search for the matching classes and ids
        if (!query) {
          result = [
            ...result,
            ...this.getSuggestionsForQuery(null, completing, "class")
                   .suggestions,
            ...this.getSuggestionsForQuery(null, completing, "id")
                   .suggestions
          ];
        }

        break;

      case "null":
        nodes = this._multiFrameQuerySelectorAll(query);
        for (let node of nodes) {
          sugs.ids.set(node.id, (sugs.ids.get(node.id)|0) + 1);
          let tag = node.localName;
          sugs.tags.set(tag, (sugs.tags.get(tag)|0) + 1);
          for (let className of node.classList) {
            sugs.classes.set(className, (sugs.classes.get(className)|0) + 1);
          }
        }
        for (let [tag, count] of sugs.tags) {
          tag && result.push([tag, count]);
        }
        for (let [id, count] of sugs.ids) {
          id && result.push(["#" + id, count]);
        }
        sugs.classes.delete("");
        sugs.classes.delete(HIDDEN_CLASS);
        for (let [className, count] of sugs.classes) {
          className && result.push(["." + className, count]);
        }
    }

    // Sort by count (desc) and name (asc)
    result = result.sort((a, b) => {
      // Computed a sortable string with first the inverted count, then the name
      let sortA = (10000 - a[1]) + a[0];
      let sortB = (10000 - b[1]) + b[0];

      // Prefixing ids, classes and tags, to group results
      let firstA = a[0].substring(0, 1);
      let firstB = b[0].substring(0, 1);

      if (firstA === "#") {
        sortA = "2" + sortA;
      } else if (firstA === ".") {
        sortA = "1" + sortA;
      } else {
        sortA = "0" + sortA;
      }

      if (firstB === "#") {
        sortB = "2" + sortB;
      } else if (firstB === ".") {
        sortB = "1" + sortB;
      } else {
        sortB = "0" + sortB;
      }

      // String compare
      return sortA.localeCompare(sortB);
    });

    result.slice(0, 25);

    return {
      query: query,
      suggestions: result
    };
  },

  /**
   * Add a pseudo-class lock to a node.
   *
   * @param NodeActor node
   * @param string pseudo
   *    A pseudoclass: ':hover', ':active', ':focus'
   * @param options
   *    Options object:
   *    `parents`: True if the pseudo-class should be added
   *      to parent nodes.
   *
   * @returns An empty packet.  A "pseudoClassLock" mutation will
   *    be queued for any changed nodes.
   */
  addPseudoClassLock: function (node, pseudo, options = {}) {
    if (isNodeDead(node)) {
      return;
    }

    // There can be only one node locked per pseudo, so dismiss all existing
    // ones
    for (let locked of this._activePseudoClassLocks) {
      if (DOMUtils.hasPseudoClassLock(locked.rawNode, pseudo)) {
        this._removePseudoClassLock(locked, pseudo);
      }
    }

    this._addPseudoClassLock(node, pseudo);

    if (!options.parents) {
      return;
    }

    let walker = this.getDocumentWalker(node.rawNode);
    let cur;
    while ((cur = walker.parentNode())) {
      let curNode = this._ref(cur);
      this._addPseudoClassLock(curNode, pseudo);
    }
  },

  _queuePseudoClassMutation: function (node) {
    this.queueMutation({
      target: node.actorID,
      type: "pseudoClassLock",
      pseudoClassLocks: node.writePseudoClassLocks()
    });
  },

  _addPseudoClassLock: function (node, pseudo) {
    if (node.rawNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE) {
      return false;
    }
    DOMUtils.addPseudoClassLock(node.rawNode, pseudo);
    this._activePseudoClassLocks.add(node);
    this._queuePseudoClassMutation(node);
    return true;
  },

  _installHelperSheet: function (node) {
    if (!this.installedHelpers) {
      this.installedHelpers = new WeakMap();
    }
    let win = node.rawNode.ownerDocument.defaultView;
    if (!this.installedHelpers.has(win)) {
      let { Style } = require("sdk/stylesheet/style");
      let { attach } = require("sdk/content/mod");
      let style = Style({source: HELPER_SHEET, type: "agent" });
      attach(style, win);
      this.installedHelpers.set(win, style);
    }
  },

  hideNode: function (node) {
    if (isNodeDead(node)) {
      return;
    }

    this._installHelperSheet(node);
    node.rawNode.classList.add(HIDDEN_CLASS);
  },

  unhideNode: function (node) {
    if (isNodeDead(node)) {
      return;
    }

    node.rawNode.classList.remove(HIDDEN_CLASS);
  },

  /**
   * Remove a pseudo-class lock from a node.
   *
   * @param NodeActor node
   * @param string pseudo
   *    A pseudoclass: ':hover', ':active', ':focus'
   * @param options
   *    Options object:
   *    `parents`: True if the pseudo-class should be removed
   *      from parent nodes.
   *
   * @returns An empty response.  "pseudoClassLock" mutations
   *    will be emitted for any changed nodes.
   */
  removePseudoClassLock: function (node, pseudo, options = {}) {
    if (isNodeDead(node)) {
      return;
    }

    this._removePseudoClassLock(node, pseudo);

    // Remove pseudo class for children as we don't want to allow
    // turning it on for some childs without setting it on some parents
    for (let locked of this._activePseudoClassLocks) {
      if (node.rawNode.contains(locked.rawNode) &&
          DOMUtils.hasPseudoClassLock(locked.rawNode, pseudo)) {
        this._removePseudoClassLock(locked, pseudo);
      }
    }

    if (!options.parents) {
      return;
    }

    let walker = this.getDocumentWalker(node.rawNode);
    let cur;
    while ((cur = walker.parentNode())) {
      let curNode = this._ref(cur);
      this._removePseudoClassLock(curNode, pseudo);
    }
  },

  _removePseudoClassLock: function (node, pseudo) {
    if (node.rawNode.nodeType != Ci.nsIDOMNode.ELEMENT_NODE) {
      return false;
    }
    DOMUtils.removePseudoClassLock(node.rawNode, pseudo);
    if (!node.writePseudoClassLocks()) {
      this._activePseudoClassLocks.delete(node);
    }

    this._queuePseudoClassMutation(node);
    return true;
  },

  /**
   * Clear all the pseudo-classes on a given node or all nodes.
   * @param {NodeActor} node Optional node to clear pseudo-classes on
   */
  clearPseudoClassLocks: function (node) {
    if (node && isNodeDead(node)) {
      return;
    }

    if (node) {
      DOMUtils.clearPseudoClassLocks(node.rawNode);
      this._activePseudoClassLocks.delete(node);
      this._queuePseudoClassMutation(node);
    } else {
      for (let locked of this._activePseudoClassLocks) {
        DOMUtils.clearPseudoClassLocks(locked.rawNode);
        this._activePseudoClassLocks.delete(locked);
        this._queuePseudoClassMutation(locked);
      }
    }
  },

  /**
   * Get a node's innerHTML property.
   */
  innerHTML: function (node) {
    let html = "";
    if (!isNodeDead(node)) {
      html = node.rawNode.innerHTML;
    }
    return LongStringActor(this.conn, html);
  },

  /**
   * Set a node's innerHTML property.
   *
   * @param {NodeActor} node The node.
   * @param {string} value The piece of HTML content.
   */
  setInnerHTML: function (node, value) {
    if (isNodeDead(node)) {
      return;
    }

    let rawNode = node.rawNode;
    if (rawNode.nodeType !== rawNode.ownerDocument.ELEMENT_NODE) {
      throw new Error("Can only change innerHTML to element nodes");
    }
    rawNode.innerHTML = value;
  },

  /**
   * Get a node's outerHTML property.
   *
   * @param {NodeActor} node The node.
   */
  outerHTML: function (node) {
    let outerHTML = "";
    if (!isNodeDead(node)) {
      outerHTML = node.rawNode.outerHTML;
    }
    return LongStringActor(this.conn, outerHTML);
  },

  /**
   * Set a node's outerHTML property.
   *
   * @param {NodeActor} node The node.
   * @param {string} value The piece of HTML content.
   */
  setOuterHTML: function (node, value) {
    if (isNodeDead(node)) {
      return;
    }

    let parsedDOM = DOMParser.parseFromString(value, "text/html");
    let rawNode = node.rawNode;
    let parentNode = rawNode.parentNode;

    // Special case for head and body.  Setting document.body.outerHTML
    // creates an extra <head> tag, and document.head.outerHTML creates
    // an extra <body>.  So instead we will call replaceChild with the
    // parsed DOM, assuming that they aren't trying to set both tags at once.
    if (rawNode.tagName === "BODY") {
      if (parsedDOM.head.innerHTML === "") {
        parentNode.replaceChild(parsedDOM.body, rawNode);
      } else {
        rawNode.outerHTML = value;
      }
    } else if (rawNode.tagName === "HEAD") {
      if (parsedDOM.body.innerHTML === "") {
        parentNode.replaceChild(parsedDOM.head, rawNode);
      } else {
        rawNode.outerHTML = value;
      }
    } else if (node.isDocumentElement()) {
      // Unable to set outerHTML on the document element.  Fall back by
      // setting attributes manually, then replace the body and head elements.
      let finalAttributeModifications = [];
      let attributeModifications = {};
      for (let attribute of rawNode.attributes) {
        attributeModifications[attribute.name] = null;
      }
      for (let attribute of parsedDOM.documentElement.attributes) {
        attributeModifications[attribute.name] = attribute.value;
      }
      for (let key in attributeModifications) {
        finalAttributeModifications.push({
          attributeName: key,
          newValue: attributeModifications[key]
        });
      }
      node.modifyAttributes(finalAttributeModifications);
      rawNode.replaceChild(parsedDOM.head, rawNode.querySelector("head"));
      rawNode.replaceChild(parsedDOM.body, rawNode.querySelector("body"));
    } else {
      rawNode.outerHTML = value;
    }
  },

  /**
   * Insert adjacent HTML to a node.
   *
   * @param {Node} node
   * @param {string} position One of "beforeBegin", "afterBegin", "beforeEnd",
   *                          "afterEnd" (see Element.insertAdjacentHTML).
   * @param {string} value The HTML content.
   */
  insertAdjacentHTML: function (node, position, value) {
    if (isNodeDead(node)) {
      return {node: [], newParents: []};
    }

    let rawNode = node.rawNode;
    let isInsertAsSibling = position === "beforeBegin" ||
      position === "afterEnd";

    // Don't insert anything adjacent to the document element.
    if (isInsertAsSibling && node.isDocumentElement()) {
      throw new Error("Can't insert adjacent element to the root.");
    }

    let rawParentNode = rawNode.parentNode;
    if (!rawParentNode && isInsertAsSibling) {
      throw new Error("Can't insert as sibling without parent node.");
    }

    // We can't use insertAdjacentHTML, because we want to return the nodes
    // being created (so the front can remove them if the user undoes
    // the change). So instead, use Range.createContextualFragment().
    let range = rawNode.ownerDocument.createRange();
    if (position === "beforeBegin" || position === "afterEnd") {
      range.selectNode(rawNode);
    } else {
      range.selectNodeContents(rawNode);
    }
    let docFrag = range.createContextualFragment(value);
    let newRawNodes = Array.from(docFrag.childNodes);
    switch (position) {
      case "beforeBegin":
        rawParentNode.insertBefore(docFrag, rawNode);
        break;
      case "afterEnd":
        // Note: if the second argument is null, rawParentNode.insertBefore
        // behaves like rawParentNode.appendChild.
        rawParentNode.insertBefore(docFrag, rawNode.nextSibling);
        break;
      case "afterBegin":
        rawNode.insertBefore(docFrag, rawNode.firstChild);
        break;
      case "beforeEnd":
        rawNode.appendChild(docFrag);
        break;
      default:
        throw new Error("Invalid position value. Must be either " +
          "'beforeBegin', 'beforeEnd', 'afterBegin' or 'afterEnd'.");
    }

    return this.attachElements(newRawNodes);
  },

  /**
   * Duplicate a specified node
   *
   * @param {NodeActor} node The node to duplicate.
   */
  duplicateNode: function ({rawNode}) {
    let clonedNode = rawNode.cloneNode(true);
    rawNode.parentNode.insertBefore(clonedNode, rawNode.nextSibling);
  },

  /**
   * Test whether a node is a document or a document element.
   *
   * @param {NodeActor} node The node to remove.
   * @return {boolean} True if the node is a document or a document element.
   */
  isDocumentOrDocumentElementNode: function (node) {
    return ((node.rawNode.ownerDocument &&
      node.rawNode.ownerDocument.documentElement === this.rawNode) ||
      node.rawNode.nodeType === Ci.nsIDOMNode.DOCUMENT_NODE);
  },

  /**
   * Removes a node from its parent node.
   *
   * @param {NodeActor} node The node to remove.
   * @returns The node's nextSibling before it was removed.
   */
  removeNode: function (node) {
    if (isNodeDead(node) || this.isDocumentOrDocumentElementNode(node)) {
      throw Error("Cannot remove document, document elements or dead nodes.");
    }

    let nextSibling = this.nextSibling(node);
    node.rawNode.remove();
    // Mutation events will take care of the rest.
    return nextSibling;
  },

  /**
   * Removes an array of nodes from their parent node.
   *
   * @param {NodeActor[]} nodes The nodes to remove.
   */
  removeNodes: function (nodes) {
    // Check that all nodes are valid before processing the removals.
    for (let node of nodes) {
      if (isNodeDead(node) || this.isDocumentOrDocumentElementNode(node)) {
        throw Error("Cannot remove document, document elements or dead nodes");
      }
    }

    for (let node of nodes) {
      node.rawNode.remove();
      // Mutation events will take care of the rest.
    }
  },

  /**
   * Insert a node into the DOM.
   */
  insertBefore: function (node, parent, sibling) {
    if (isNodeDead(node) ||
        isNodeDead(parent) ||
        (sibling && isNodeDead(sibling))) {
      return;
    }

    let rawNode = node.rawNode;
    let rawParent = parent.rawNode;
    let rawSibling = sibling ? sibling.rawNode : null;

    // Don't bother inserting a node if the document position isn't going
    // to change. This prevents needless iframes reloading and mutations.
    if (rawNode.parentNode === rawParent) {
      let currentNextSibling = this.nextSibling(node);
      currentNextSibling = currentNextSibling ? currentNextSibling.rawNode :
                                                null;

      if (rawNode === rawSibling || currentNextSibling === rawSibling) {
        return;
      }
    }

    rawParent.insertBefore(rawNode, rawSibling);
  },

  /**
   * Editing a node's tagname actually means creating a new node with the same
   * attributes, removing the node and inserting the new one instead.
   * This method does not return anything as mutation events are taking care of
   * informing the consumers about changes.
   */
  editTagName: function (node, tagName) {
    if (isNodeDead(node)) {
      return null;
    }

    let oldNode = node.rawNode;

    // Create a new element with the same attributes as the current element and
    // prepare to replace the current node with it.
    let newNode;
    try {
      newNode = nodeDocument(oldNode).createElement(tagName);
    } catch (x) {
      // Failed to create a new element with that tag name, ignore the change,
      // and signal the error to the front.
      return Promise.reject(new Error("Could not change node's tagName to " + tagName));
    }

    let attrs = oldNode.attributes;
    for (let i = 0; i < attrs.length; i++) {
      newNode.setAttribute(attrs[i].name, attrs[i].value);
    }

    // Insert the new node, and transfer the old node's children.
    oldNode.parentNode.insertBefore(newNode, oldNode);
    while (oldNode.firstChild) {
      newNode.appendChild(oldNode.firstChild);
    }

    oldNode.remove();
    return null;
  },

  /**
   * Get any pending mutation records.  Must be called by the client after
   * the `new-mutations` notification is received.  Returns an array of
   * mutation records.
   *
   * Mutation records have a basic structure:
   *
   * {
   *   type: attributes|characterData|childList,
   *   target: <domnode actor ID>,
   * }
   *
   * And additional attributes based on the mutation type:
   *
   * `attributes` type:
   *   attributeName: <string> - the attribute that changed
   *   attributeNamespace: <string> - the attribute's namespace URI, if any.
   *   newValue: <string> - The new value of the attribute, if any.
   *
   * `characterData` type:
   *   newValue: <string> - the new nodeValue for the node
   *
   * `childList` type is returned when the set of children for a node
   * has changed.  Includes extra data, which can be used by the client to
   * maintain its ownership subtree.
   *
   *   added: array of <domnode actor ID> - The list of actors *previously
   *     seen by the client* that were added to the target node.
   *   removed: array of <domnode actor ID> The list of actors *previously
   *     seen by the client* that were removed from the target node.
   *   inlineTextChild: If the node now has a single text child, it will
   *     be sent here.
   *
   * Actors that are included in a MutationRecord's `removed` but
   * not in an `added` have been removed from the client's ownership
   * tree (either by being moved under a node the client has seen yet
   * or by being removed from the tree entirely), and is considered
   * 'orphaned'.
   *
   * Keep in mind that if a node that the client hasn't seen is moved
   * into or out of the target node, it will not be included in the
   * removedNodes and addedNodes list, so if the client is interested
   * in the new set of children it needs to issue a `children` request.
   */
  getMutations: function (options = {}) {
    let pending = this._pendingMutations || [];
    this._pendingMutations = [];

    if (options.cleanup) {
      for (let node of this._orphaned) {
        // Release the orphaned node.  Nodes or children that have been
        // retained will be moved to this._retainedOrphans.
        this.releaseNode(node);
      }
      this._orphaned = new Set();
    }

    return pending;
  },

  queueMutation: function (mutation) {
    if (!this.actorID || this._destroyed) {
      // We've been destroyed, don't bother queueing this mutation.
      return;
    }
    // We only send the `new-mutations` notification once, until the client
    // fetches mutations with the `getMutations` packet.
    let needEvent = this._pendingMutations.length === 0;

    this._pendingMutations.push(mutation);

    if (needEvent) {
      events.emit(this, "new-mutations");
    }
  },

  /**
   * Handles mutations from the DOM mutation observer API.
   *
   * @param array[MutationRecord] mutations
   *    See https://developer.mozilla.org/en-US/docs/Web/API/MutationObserver#MutationRecord
   */
  onMutations: function (mutations) {
    // Notify any observers that want *all* mutations (even on nodes that aren't
    // referenced).  This is not sent over the protocol so can only be used by
    // scripts running in the server process.
    events.emit(this, "any-mutation");

    for (let change of mutations) {
      let targetActor = this.getNode(change.target);
      if (!targetActor) {
        continue;
      }
      let targetNode = change.target;
      let type = change.type;
      let mutation = {
        type: type,
        target: targetActor.actorID,
      };

      if (type === "attributes") {
        mutation.attributeName = change.attributeName;
        mutation.attributeNamespace = change.attributeNamespace || undefined;
        mutation.newValue = targetNode.hasAttribute(mutation.attributeName) ?
                            targetNode.getAttribute(mutation.attributeName)
                            : null;
      } else if (type === "characterData") {
        mutation.newValue = targetNode.nodeValue;
        this._maybeQueueInlineTextChildMutation(change, targetNode);
      } else if (type === "childList" || type === "nativeAnonymousChildList") {
        // Get the list of removed and added actors that the client has seen
        // so that it can keep its ownership tree up to date.
        let removedActors = [];
        let addedActors = [];
        for (let removed of change.removedNodes) {
          let removedActor = this.getNode(removed);
          if (!removedActor) {
            // If the client never encountered this actor we don't need to
            // mention that it was removed.
            continue;
          }
          // While removed from the tree, nodes are saved as orphaned.
          this._orphaned.add(removedActor);
          removedActors.push(removedActor.actorID);
        }
        for (let added of change.addedNodes) {
          let addedActor = this.getNode(added);
          if (!addedActor) {
            // If the client never encounted this actor we don't need to tell
            // it about its addition for ownership tree purposes - if the
            // client wants to see the new nodes it can ask for children.
            continue;
          }
          // The actor is reconnected to the ownership tree, unorphan
          // it and let the client know so that its ownership tree is up
          // to date.
          this._orphaned.delete(addedActor);
          addedActors.push(addedActor.actorID);
        }

        mutation.numChildren = targetActor.numChildren;
        mutation.removed = removedActors;
        mutation.added = addedActors;

        let inlineTextChild = this.inlineTextChild(targetActor);
        if (inlineTextChild) {
          mutation.inlineTextChild = inlineTextChild.form();
        }
      }
      this.queueMutation(mutation);
    }
  },

  /**
   * Check if the provided mutation could change the way the target element is
   * inlined with its parent node. If it might, a custom mutation of type
   * "inlineTextChild" will be queued.
   *
   * @param {MutationRecord} mutation
   *        A characterData type mutation
   */
  _maybeQueueInlineTextChildMutation: function (mutation) {
    let {oldValue, target} = mutation;
    let newValue = target.nodeValue;
    let limit = gValueSummaryLength;

    if ((oldValue.length <= limit && newValue.length <= limit) ||
        (oldValue.length > limit && newValue.length > limit)) {
      // Bail out if the new & old values are both below/above the size limit.
      return;
    }

    let parentActor = this.getNode(target.parentNode);
    if (!parentActor || parentActor.rawNode.children.length > 0) {
      // If the parent node has other children, a character data mutation will
      // not change anything regarding inlining text nodes.
      return;
    }

    let inlineTextChild = this.inlineTextChild(parentActor);
    this.queueMutation({
      type: "inlineTextChild",
      target: parentActor.actorID,
      inlineTextChild:
        inlineTextChild ? inlineTextChild.form() : undefined
    });
  },

  onFrameLoad: function ({ window, isTopLevel }) {
    if (isTopLevel) {
      // If we initialize the inspector while the document is loading,
      // we may already have a root document set in the constructor.
      if (this.rootDoc && !Cu.isDeadWrapper(this.rootDoc) &&
          this.rootDoc.defaultView) {
        this.onFrameUnload({ window: this.rootDoc.defaultView });
      }
      this.rootDoc = window.document;
      this.rootNode = this.document();
      this.queueMutation({
        type: "newRoot",
        target: this.rootNode.form()
      });
      return;
    }
    let frame = getFrameElement(window);
    let frameActor = this.getNode(frame);
    if (!frameActor) {
      return;
    }

    this.queueMutation({
      type: "frameLoad",
      target: frameActor.actorID,
    });

    // Send a childList mutation on the frame.
    this.queueMutation({
      type: "childList",
      target: frameActor.actorID,
      added: [],
      removed: []
    });
  },

  // Returns true if domNode is in window or a subframe.
  _childOfWindow: function (window, domNode) {
    let win = nodeDocument(domNode).defaultView;
    while (win) {
      if (win === window) {
        return true;
      }
      win = getFrameElement(win);
    }
    return false;
  },

  onFrameUnload: function ({ window }) {
    // Any retained orphans that belong to this document
    // or its children need to be released, and a mutation sent
    // to notify of that.
    let releasedOrphans = [];

    for (let retained of this._retainedOrphans) {
      if (Cu.isDeadWrapper(retained.rawNode) ||
          this._childOfWindow(window, retained.rawNode)) {
        this._retainedOrphans.delete(retained);
        releasedOrphans.push(retained.actorID);
        this.releaseNode(retained, { force: true });
      }
    }

    if (releasedOrphans.length > 0) {
      this.queueMutation({
        target: this.rootNode.actorID,
        type: "unretained",
        nodes: releasedOrphans
      });
    }

    let doc = window.document;
    let documentActor = this.getNode(doc);
    if (!documentActor) {
      return;
    }

    if (this.rootDoc === doc) {
      this.rootDoc = null;
      this.rootNode = null;
    }

    this.queueMutation({
      type: "documentUnload",
      target: documentActor.actorID
    });

    let walker = this.getDocumentWalker(doc);
    let parentNode = walker.parentNode();
    if (parentNode) {
      // Send a childList mutation on the frame so that clients know
      // they should reread the children list.
      this.queueMutation({
        type: "childList",
        target: this.getNode(parentNode).actorID,
        added: [],
        removed: []
      });
    }

    // Need to force a release of this node, because those nodes can't
    // be accessed anymore.
    this.releaseNode(documentActor, { force: true });
  },

  /**
   * Check if a node is attached to the DOM tree of the current page.
   * @param {nsIDomNode} rawNode
   * @return {Boolean} false if the node is removed from the tree or within a
   * document fragment
   */
  _isInDOMTree: function (rawNode) {
    let walker = this.getDocumentWalker(rawNode);
    let current = walker.currentNode;

    // Reaching the top of tree
    while (walker.parentNode()) {
      current = walker.currentNode;
    }

    // The top of the tree is a fragment or is not rootDoc, hence rawNode isn't
    // attached
    if (current.nodeType === Ci.nsIDOMNode.DOCUMENT_FRAGMENT_NODE ||
        current !== this.rootDoc) {
      return false;
    }

    // Otherwise the top of the tree is rootDoc, hence rawNode is in rootDoc
    return true;
  },

  /**
   * @see _isInDomTree
   */
  isInDOMTree: function (node) {
    if (isNodeDead(node)) {
      return false;
    }
    return this._isInDOMTree(node.rawNode);
  },

  /**
   * Given an ObjectActor (identified by its ID), commonly used in the debugger,
   * webconsole and variablesView, return the corresponding inspector's
   * NodeActor
   */
  getNodeActorFromObjectActor: function (objectActorID) {
    let actor = this.conn.getActor(objectActorID);
    if (!actor) {
      return null;
    }

    let debuggerObject = this.conn.getActor(objectActorID).obj;
    let rawNode = debuggerObject.unsafeDereference();

    if (!this._isInDOMTree(rawNode)) {
      return null;
    }

    // This is a special case for the document object whereby it is considered
    // as document.documentElement (the <html> node)
    if (rawNode.defaultView && rawNode === rawNode.defaultView.document) {
      rawNode = rawNode.documentElement;
    }

    return this.attachElement(rawNode);
  },

  /**
   * Given a StyleSheetActor (identified by its ID), commonly used in the
   * style-editor, get its ownerNode and return the corresponding walker's
   * NodeActor.
   * Note that getNodeFromActor was added later and can now be used instead.
   */
  getStyleSheetOwnerNode: function (styleSheetActorID) {
    return this.getNodeFromActor(styleSheetActorID, ["ownerNode"]);
  },

  /**
   * This method can be used to retrieve NodeActor for DOM nodes from other
   * actors in a way that they can later be highlighted in the page, or
   * selected in the inspector.
   * If an actor has a reference to a DOM node, and the UI needs to know about
   * this DOM node (and possibly select it in the inspector), the UI should
   * first retrieve a reference to the walkerFront:
   *
   * // Make sure the inspector/walker have been initialized first.
   * toolbox.initInspector().then(() => {
   *  // Retrieve the walker.
   *  let walker = toolbox.walker;
   * });
   *
   * And then call this method:
   *
   * // Get the nodeFront from my actor, passing the ID and properties path.
   * walker.getNodeFromActor(myActorID, ["element"]).then(nodeFront => {
   *   // Use the nodeFront, e.g. select the node in the inspector.
   *   toolbox.getPanel("inspector").selection.setNodeFront(nodeFront);
   * });
   *
   * @param {String} actorID The ID for the actor that has a reference to the
   * DOM node.
   * @param {Array} path Where, on the actor, is the DOM node stored. If in the
   * scope of the actor, the node is available as `this.data.node`, then this
   * should be ["data", "node"].
   * @return {NodeActor} The attached NodeActor, or null if it couldn't be
   * found.
   */
  getNodeFromActor: function (actorID, path) {
    let actor = this.conn.getActor(actorID);
    if (!actor) {
      return null;
    }

    let obj = actor;
    for (let name of path) {
      if (!(name in obj)) {
        return null;
      }
      obj = obj[name];
    }

    return this.attachElement(obj);
  },

  /**
   * Returns an instance of the LayoutActor that is used to retrieve CSS layout-related
   * information.
   *
   * @return {LayoutActor}
   */
  getLayoutInspector: function () {
    if (!this.layoutActor) {
      this.layoutActor = new LayoutActor(this.conn, this.tabActor, this);
    }

    return this.layoutActor;
  },
});

/**
 * Server side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
exports.InspectorActor = protocol.ActorClassWithSpec(inspectorSpec, {
  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;

    this._onColorPicked = this._onColorPicked.bind(this);
    this._onColorPickCanceled = this._onColorPickCanceled.bind(this);
    this.destroyEyeDropper = this.destroyEyeDropper.bind(this);
  },

  destroy: function () {
    protocol.Actor.prototype.destroy.call(this);

    this.destroyEyeDropper();

    this._highlighterPromise = null;
    this._pageStylePromise = null;
    this._walkerPromise = null;
    this.walker = null;
    this.tabActor = null;
  },

  get window() {
    return this.tabActor.window;
  },

  getWalker: function (options = {}) {
    if (this._walkerPromise) {
      return this._walkerPromise;
    }

    let deferred = promise.defer();
    this._walkerPromise = deferred.promise;

    let window = this.window;
    let domReady = () => {
      let tabActor = this.tabActor;
      window.removeEventListener("DOMContentLoaded", domReady, true);
      this.walker = WalkerActor(this.conn, tabActor, options);
      this.manage(this.walker);
      events.once(this.walker, "destroyed", () => {
        this._walkerPromise = null;
        this._pageStylePromise = null;
      });
      deferred.resolve(this.walker);
    };

    if (window.document.readyState === "loading") {
      window.addEventListener("DOMContentLoaded", domReady, true);
    } else {
      domReady();
    }

    return this._walkerPromise;
  },

  getPageStyle: function () {
    if (this._pageStylePromise) {
      return this._pageStylePromise;
    }

    this._pageStylePromise = this.getWalker().then(walker => {
      let pageStyle = PageStyleActor(this);
      this.manage(pageStyle);
      return pageStyle;
    });
    return this._pageStylePromise;
  },

  /**
   * The most used highlighter actor is the HighlighterActor which can be
   * conveniently retrieved by this method.
   * The same instance will always be returned by this method when called
   * several times.
   * The highlighter actor returned here is used to highlighter elements's
   * box-models from the markup-view, box model, console, debugger, ... as
   * well as select elements with the pointer (pick).
   *
   * @param {Boolean} autohide Optionally autohide the highlighter after an
   * element has been picked
   * @return {HighlighterActor}
   */
  getHighlighter: function (autohide) {
    if (this._highlighterPromise) {
      return this._highlighterPromise;
    }

    this._highlighterPromise = this.getWalker().then(walker => {
      let highlighter = HighlighterActor(this, autohide);
      this.manage(highlighter);
      return highlighter;
    });
    return this._highlighterPromise;
  },

  /**
   * If consumers need to display several highlighters at the same time or
   * different types of highlighters, then this method should be used, passing
   * the type name of the highlighter needed as argument.
   * A new instance will be created everytime the method is called, so it's up
   * to the consumer to release it when it is not needed anymore
   *
   * @param {String} type The type of highlighter to create
   * @return {Highlighter} The highlighter actor instance or null if the
   * typeName passed doesn't match any available highlighter
   */
  getHighlighterByType: function (typeName) {
    if (isTypeRegistered(typeName)) {
      return CustomHighlighterActor(this, typeName);
    }
    return null;
  },

  /**
   * Get the node's image data if any (for canvas and img nodes).
   * Returns an imageData object with the actual data being a LongStringActor
   * and a size json object.
   * The image data is transmitted as a base64 encoded png data-uri.
   * The method rejects if the node isn't an image or if the image is missing
   *
   * Accepts a maxDim request parameter to resize images that are larger. This
   * is important as the resizing occurs server-side so that image-data being
   * transfered in the longstring back to the client will be that much smaller
   */
  getImageDataFromURL: function (url, maxDim) {
    let img = new this.window.Image();
    img.src = url;

    // imageToImageData waits for the image to load.
    return imageToImageData(img, maxDim).then(imageData => {
      return {
        data: LongStringActor(this.conn, imageData.data),
        size: imageData.size
      };
    });
  },

  /**
   * Resolve a URL to its absolute form, in the scope of a given content window.
   * @param {String} url.
   * @param {NodeActor} node If provided, the owner window of this node will be
   * used to resolve the URL. Otherwise, the top-level content window will be
   * used instead.
   * @return {String} url.
   */
  resolveRelativeURL: function (url, node) {
    let document = isNodeDead(node)
                   ? this.window.document
                   : nodeDocument(node.rawNode);

    if (!document) {
      return url;
    }

    let baseURI = Services.io.newURI(document.location.href, null, null);
    return Services.io.newURI(url, null, baseURI).spec;
  },

  /**
   * Create an instance of the eye-dropper highlighter and store it on this._eyeDropper.
   * Note that for now, a new instance is created every time to deal with page navigation.
   */
  createEyeDropper: function () {
    this.destroyEyeDropper();
    this._highlighterEnv = new HighlighterEnvironment();
    this._highlighterEnv.initFromTabActor(this.tabActor);
    this._eyeDropper = new EyeDropper(this._highlighterEnv);
  },

  /**
   * Destroy the current eye-dropper highlighter instance.
   */
  destroyEyeDropper: function () {
    if (this._eyeDropper) {
      this.cancelPickColorFromPage();
      this._eyeDropper.destroy();
      this._eyeDropper = null;
      this._highlighterEnv.destroy();
      this._highlighterEnv = null;
    }
  },

  /**
   * Pick a color from the page using the eye-dropper. This method doesn't return anything
   * but will cause events to be sent to the front when a color is picked or when the user
   * cancels the picker.
   * @param {Object} options
   */
  pickColorFromPage: function (options) {
    this.createEyeDropper();
    this._eyeDropper.show(this.window.document.documentElement, options);
    this._eyeDropper.once("selected", this._onColorPicked);
    this._eyeDropper.once("canceled", this._onColorPickCanceled);
    events.once(this.tabActor, "will-navigate", this.destroyEyeDropper);
  },

  /**
   * After the pickColorFromPage method is called, the only way to dismiss the eye-dropper
   * highlighter is for the user to click in the page and select a color. If you need to
   * dismiss the eye-dropper programatically instead, use this method.
   */
  cancelPickColorFromPage: function () {
    if (this._eyeDropper) {
      this._eyeDropper.hide();
      this._eyeDropper.off("selected", this._onColorPicked);
      this._eyeDropper.off("canceled", this._onColorPickCanceled);
      events.off(this.tabActor, "will-navigate", this.destroyEyeDropper);
    }
  },

  _onColorPicked: function (e, color) {
    events.emit(this, "color-picked", color);
  },

  _onColorPickCanceled: function () {
    events.emit(this, "color-pick-canceled");
  }
});

// Exported for test purposes.
exports._documentWalker = DocumentWalker;

function nodeDocument(node) {
  if (Cu.isDeadWrapper(node)) {
    return null;
  }
  return node.ownerDocument ||
         (node.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE ? node : null);
}

function nodeDocshell(node) {
  let doc = node ? nodeDocument(node) : null;
  let win = doc ? doc.defaultView : null;
  if (win) {
    return win.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIDocShell);
  }
  return null;
}

function isNodeDead(node) {
  return !node || !node.rawNode || Cu.isDeadWrapper(node.rawNode);
}

/**
 * Wrapper for inDeepTreeWalker.  Adds filtering to the traversal methods.
 * See inDeepTreeWalker for more information about the methods.
 *
 * @param {DOMNode} node
 * @param {Window} rootWin
 * @param {Int} whatToShow See nodeFilterConstants / inIDeepTreeWalker for
 * options.
 * @param {Function} filter A custom filter function Taking in a DOMNode
 *        and returning an Int. See WalkerActor.nodeFilter for an example.
 */
function DocumentWalker(node, rootWin,
    whatToShow = nodeFilterConstants.SHOW_ALL,
    filter = standardTreeWalkerFilter) {
  if (!rootWin.location) {
    throw new Error("Got an invalid root window in DocumentWalker");
  }

  this.walker = Cc["@mozilla.org/inspector/deep-tree-walker;1"]
    .createInstance(Ci.inIDeepTreeWalker);
  this.walker.showAnonymousContent = true;
  this.walker.showSubDocuments = true;
  this.walker.showDocumentsAsNodes = true;
  this.walker.init(rootWin.document, whatToShow);
  this.filter = filter;

  // Make sure that the walker knows about the initial node (which could
  // be skipped due to a filter).  Note that simply calling parentNode()
  // causes currentNode to be updated.
  this.walker.currentNode = node;
  while (node &&
         this.filter(node) === nodeFilterConstants.FILTER_SKIP) {
    node = this.walker.parentNode();
  }
}

DocumentWalker.prototype = {
  get node() {
    return this.walker.node;
  },
  get whatToShow() {
    return this.walker.whatToShow;
  },
  get currentNode() {
    return this.walker.currentNode;
  },
  set currentNode(val) {
    this.walker.currentNode = val;
  },

  parentNode: function () {
    return this.walker.parentNode();
  },

  nextNode: function () {
    let node = this.walker.currentNode;
    if (!node) {
      return null;
    }

    let nextNode = this.walker.nextNode();
    while (nextNode &&
           this.filter(nextNode) === nodeFilterConstants.FILTER_SKIP) {
      nextNode = this.walker.nextNode();
    }

    return nextNode;
  },

  firstChild: function () {
    let node = this.walker.currentNode;
    if (!node) {
      return null;
    }

    let firstChild = this.walker.firstChild();
    while (firstChild &&
           this.filter(firstChild) === nodeFilterConstants.FILTER_SKIP) {
      firstChild = this.walker.nextSibling();
    }

    return firstChild;
  },

  lastChild: function () {
    let node = this.walker.currentNode;
    if (!node) {
      return null;
    }

    let lastChild = this.walker.lastChild();
    while (lastChild &&
           this.filter(lastChild) === nodeFilterConstants.FILTER_SKIP) {
      lastChild = this.walker.previousSibling();
    }

    return lastChild;
  },

  previousSibling: function () {
    let node = this.walker.previousSibling();
    while (node && this.filter(node) === nodeFilterConstants.FILTER_SKIP) {
      node = this.walker.previousSibling();
    }
    return node;
  },

  nextSibling: function () {
    let node = this.walker.nextSibling();
    while (node && this.filter(node) === nodeFilterConstants.FILTER_SKIP) {
      node = this.walker.nextSibling();
    }
    return node;
  }
};

function isInXULDocument(el) {
  let doc = nodeDocument(el);
  return doc &&
         doc.documentElement &&
         doc.documentElement.namespaceURI === XUL_NS;
}

/**
 * This DeepTreeWalker filter skips whitespace text nodes and anonymous
 * content with the exception of ::before and ::after and anonymous content
 * in XUL document (needed to show all elements in the browser toolbox).
 */
function standardTreeWalkerFilter(node) {
  // ::before and ::after are native anonymous content, but we always
  // want to show them
  if (node.nodeName === "_moz_generated_content_before" ||
      node.nodeName === "_moz_generated_content_after") {
    return nodeFilterConstants.FILTER_ACCEPT;
  }

  // Ignore empty whitespace text nodes that do not impact the layout.
  if (isWhitespaceTextNode(node)) {
    return nodeHasSize(node)
           ? nodeFilterConstants.FILTER_ACCEPT
           : nodeFilterConstants.FILTER_SKIP;
  }

  // Ignore all native and XBL anonymous content inside a non-XUL document
  if (!isInXULDocument(node) && (isXBLAnonymous(node) ||
                                  isNativeAnonymous(node))) {
    // Note: this will skip inspecting the contents of feedSubscribeLine since
    // that's XUL content injected in an HTML document, but we need to because
    // this also skips many other elements that need to be skipped - like form
    // controls, scrollbars, video controls, etc (see bug 1187482).
    return nodeFilterConstants.FILTER_SKIP;
  }

  return nodeFilterConstants.FILTER_ACCEPT;
}

/**
 * This DeepTreeWalker filter is like standardTreeWalkerFilter except that
 * it also includes all anonymous content (like internal form controls).
 */
function allAnonymousContentTreeWalkerFilter(node) {
  // Ignore empty whitespace text nodes that do not impact the layout.
  if (isWhitespaceTextNode(node)) {
    return nodeHasSize(node)
           ? nodeFilterConstants.FILTER_ACCEPT
           : nodeFilterConstants.FILTER_SKIP;
  }
  return nodeFilterConstants.FILTER_ACCEPT;
}

/**
 * Is the given node a text node composed of whitespace only?
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isWhitespaceTextNode(node) {
  return node.nodeType == Ci.nsIDOMNode.TEXT_NODE && !/[^\s]/.exec(node.nodeValue);
}

/**
 * Does the given node have non-0 width and height?
 * @param {DOMNode} node
 * @return {Boolean}
 */
function nodeHasSize(node) {
  if (!node.getBoxQuads) {
    return false;
  }

  let quads = node.getBoxQuads();
  return quads.length && quads.some(quad => quad.bounds.width && quad.bounds.height);
}

/**
 * Returns a promise that is settled once the given HTMLImageElement has
 * finished loading.
 *
 * @param {HTMLImageElement} image - The image element.
 * @param {Number} timeout - Maximum amount of time the image is allowed to load
 * before the waiting is aborted. Ignored if flags.testing is set.
 *
 * @return {Promise} that is fulfilled once the image has loaded. If the image
 * fails to load or the load takes too long, the promise is rejected.
 */
function ensureImageLoaded(image, timeout) {
  let { HTMLImageElement } = image.ownerDocument.defaultView;
  if (!(image instanceof HTMLImageElement)) {
    return promise.reject("image must be an HTMLImageELement");
  }

  if (image.complete) {
    // The image has already finished loading.
    return promise.resolve();
  }

  // This image is still loading.
  let onLoad = AsyncUtils.listenOnce(image, "load");

  // Reject if loading fails.
  let onError = AsyncUtils.listenOnce(image, "error").then(() => {
    return promise.reject("Image '" + image.src + "' failed to load.");
  });

  // Don't timeout when testing. This is never settled.
  let onAbort = new Promise(() => {});

  if (!flags.testing) {
    // Tests are not running. Reject the promise after given timeout.
    onAbort = DevToolsUtils.waitForTime(timeout).then(() => {
      return promise.reject("Image '" + image.src + "' took too long to load.");
    });
  }

  // See which happens first.
  return promise.race([onLoad, onError, onAbort]);
}

/**
 * Given an <img> or <canvas> element, return the image data-uri. If @param node
 * is an <img> element, the method waits a while for the image to load before
 * the data is generated. If the image does not finish loading in a reasonable
 * time (IMAGE_FETCHING_TIMEOUT milliseconds) the process aborts.
 *
 * @param {HTMLImageElement|HTMLCanvasElement} node - The <img> or <canvas>
 * element, or Image() object. Other types cause the method to reject.
 * @param {Number} maxDim - Optionally pass a maximum size you want the longest
 * side of the image to be resized to before getting the image data.

 * @return {Promise} A promise that is fulfilled with an object containing the
 * data-uri and size-related information:
 * { data: "...",
 *   size: {
 *     naturalWidth: 400,
 *     naturalHeight: 300,
 *     resized: true }
 *  }.
 *
 * If something goes wrong, the promise is rejected.
 */
var imageToImageData = Task.async(function* (node, maxDim) {
  let { HTMLCanvasElement, HTMLImageElement } = node.ownerDocument.defaultView;

  let isImg = node instanceof HTMLImageElement;
  let isCanvas = node instanceof HTMLCanvasElement;

  if (!isImg && !isCanvas) {
    throw new Error("node is not a <canvas> or <img> element.");
  }

  if (isImg) {
    // Ensure that the image is ready.
    yield ensureImageLoaded(node, IMAGE_FETCHING_TIMEOUT);
  }

  // Get the image resize ratio if a maxDim was provided
  let resizeRatio = 1;
  let imgWidth = node.naturalWidth || node.width;
  let imgHeight = node.naturalHeight || node.height;
  let imgMax = Math.max(imgWidth, imgHeight);
  if (maxDim && imgMax > maxDim) {
    resizeRatio = maxDim / imgMax;
  }

  // Extract the image data
  let imageData;
  // The image may already be a data-uri, in which case, save ourselves the
  // trouble of converting via the canvas.drawImage.toDataURL method, but only
  // if the image doesn't need resizing
  if (isImg && node.src.startsWith("data:") && resizeRatio === 1) {
    imageData = node.src;
  } else {
    // Create a canvas to copy the rawNode into and get the imageData from
    let canvas = node.ownerDocument.createElementNS(XHTML_NS, "canvas");
    canvas.width = imgWidth * resizeRatio;
    canvas.height = imgHeight * resizeRatio;
    let ctx = canvas.getContext("2d");

    // Copy the rawNode image or canvas in the new canvas and extract data
    ctx.drawImage(node, 0, 0, canvas.width, canvas.height);
    imageData = canvas.toDataURL("image/png");
  }

  return {
    data: imageData,
    size: {
      naturalWidth: imgWidth,
      naturalHeight: imgHeight,
      resized: resizeRatio !== 1
    }
  };
});

loader.lazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});
