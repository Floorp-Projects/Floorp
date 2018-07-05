/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const Services = require("Services");
const InspectorUtils = require("InspectorUtils");
const protocol = require("devtools/shared/protocol");
const { nodeSpec, nodeListSpec } = require("devtools/shared/specs/node");

loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);

loader.lazyRequireGetter(this, "getCssPath", "devtools/shared/inspector/css-logic", true);
loader.lazyRequireGetter(this, "getXPath", "devtools/shared/inspector/css-logic", true);
loader.lazyRequireGetter(this, "findCssSelector", "devtools/shared/inspector/css-logic", true);

loader.lazyRequireGetter(this, "isAfterPseudoElement", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isAnonymous", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isBeforePseudoElement", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isDirectShadowHostChild", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isNativeAnonymous", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isShadowAnonymous", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isShadowHost", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isShadowRoot", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isXBLAnonymous", "devtools/shared/layout/utils", true);

loader.lazyRequireGetter(this, "InspectorActorUtils", "devtools/server/actors/inspector/utils");
loader.lazyRequireGetter(this, "LongStringActor", "devtools/server/actors/string", true);
loader.lazyRequireGetter(this, "getFontPreviewData", "devtools/server/actors/styles", true);
loader.lazyRequireGetter(this, "CssLogic", "devtools/server/actors/inspector/css-logic", true);
loader.lazyRequireGetter(this, "EventParsers", "devtools/server/actors/inspector/event-parsers", true);

const SUBGRID_ENABLED =
  Services.prefs.getBoolPref("layout.css.grid-template-subgrid-value.enabled");

const PSEUDO_CLASSES = [":hover", ":active", ":focus"];
const FONT_FAMILY_PREVIEW_TEXT = "The quick brown fox jumps over the lazy dog";
const FONT_FAMILY_PREVIEW_TEXT_SIZE = 20;

/**
 * Server side of the node actor.
 */
const NodeActor = protocol.ActorClassWithSpec(nodeSpec, {
  initialize: function(walker, node) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.walker = walker;
    this.rawNode = node;
    this._eventParsers = new EventParsers().parsers;

    // Store the original display type and whether or not the node is displayed to
    // track changes when reflows occur.
    this.currentDisplayType = this.displayType;
    this.wasDisplayed = this.isDisplayed;
  },

  toString: function() {
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

  isDocumentElement: function() {
    return this.rawNode.ownerDocument &&
           this.rawNode.ownerDocument.documentElement === this.rawNode;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);

    if (this.mutationObserver) {
      if (!Cu.isDeadWrapper(this.mutationObserver)) {
        this.mutationObserver.disconnect();
      }
      this.mutationObserver = null;
    }

    if (this.slotchangeListener) {
      if (!InspectorActorUtils.isNodeDead(this)) {
        this.rawNode.removeEventListener("slotchange", this.slotchangeListener);
      }
      this.slotchangeListener = null;
    }

    this.rawNode = null;
    this.walker = null;
  },

  // Returns the JSON representation of this object over the wire.
  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    const parentNode = this.walker.parentNode(this);
    const inlineTextChild = this.walker.inlineTextChild(this);
    const shadowRoot = isShadowRoot(this.rawNode);
    const hostActor = shadowRoot ? this.walker.getNode(this.rawNode.host) : null;

    const form = {
      actor: this.actorID,
      host: hostActor ? hostActor.actorID : undefined,
      baseURI: this.rawNode.baseURI,
      parent: parentNode ? parentNode.actorID : undefined,
      nodeType: this.rawNode.nodeType,
      namespaceURI: this.rawNode.namespaceURI,
      nodeName: this.rawNode.nodeName,
      nodeValue: this.rawNode.nodeValue,
      displayName: InspectorActorUtils.getNodeDisplayName(this.rawNode),
      numChildren: this.numChildren,
      inlineTextChild: inlineTextChild ? inlineTextChild.form() : undefined,
      displayType: this.displayType,

      // doctype attributes
      name: this.rawNode.name,
      publicId: this.rawNode.publicId,
      systemId: this.rawNode.systemId,

      attrs: this.writeAttrs(),
      isBeforePseudoElement: isBeforePseudoElement(this.rawNode),
      isAfterPseudoElement: isAfterPseudoElement(this.rawNode),
      isAnonymous: isAnonymous(this.rawNode),
      isNativeAnonymous: isNativeAnonymous(this.rawNode),
      isXBLAnonymous: isXBLAnonymous(this.rawNode),
      isShadowAnonymous: isShadowAnonymous(this.rawNode),
      isShadowRoot: shadowRoot,
      isShadowHost: isShadowHost(this.rawNode),
      isDirectShadowHostChild: isDirectShadowHostChild(this.rawNode),
      pseudoClassLocks: this.writePseudoClassLocks(),

      isDisplayed: this.isDisplayed,
      isInHTMLDocument: this.rawNode.ownerDocument &&
        this.rawNode.ownerDocument.contentType === "text/html",
      hasEventListeners: this._hasEventListeners,
    };

    if (this.isDocumentElement()) {
      form.isDocumentElement = true;
    }

    return form;
  },

  /**
   * Watch the given document node for mutations using the DOM observer
   * API.
   */
  watchDocument: function(doc, callback) {
    const node = this.rawNode;
    // Create the observer on the node's actor.  The node will make sure
    // the observer is cleaned up when the actor is released.
    const observer = new doc.defaultView.MutationObserver(callback);
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

  /**
   * Watch for all "slotchange" events on the node.
   */
  watchSlotchange: function(callback) {
    this.slotchangeListener = callback;
    this.rawNode.addEventListener("slotchange", this.slotchangeListener);
  },

  // Estimate the number of children that the walker will return without making
  // a call to children() if possible.
  get numChildren() {
    // For pseudo elements, childNodes.length returns 1, but the walker
    // will return 0.
    if (isBeforePseudoElement(this.rawNode) || isAfterPseudoElement(this.rawNode)) {
      return 0;
    }

    const rawNode = this.rawNode;
    let numChildren = rawNode.childNodes.length;
    const hasAnonChildren = rawNode.nodeType === Node.ELEMENT_NODE &&
                          rawNode.ownerDocument.getAnonymousNodes(rawNode);

    const hasContentDocument = rawNode.contentDocument;
    const hasSVGDocument = rawNode.getSVGDocument && rawNode.getSVGDocument();
    if (numChildren === 0 && (hasContentDocument || hasSVGDocument)) {
      // This might be an iframe with virtual children.
      numChildren = 1;
    }

    // Normal counting misses ::before/::after.  Also, some anonymous children
    // may ultimately be skipped, so we have to consult with the walker.
    if (numChildren === 0 || hasAnonChildren || isShadowHost(this.rawNode)) {
      numChildren = this.walker.countChildren(this);
    }

    return numChildren;
  },

  get computedStyle() {
    if (!this._computedStyle) {
      this._computedStyle = CssLogic.getComputedStyle(this.rawNode);
    }
    return this._computedStyle;
  },

  /**
   * Returns the computed display style property value of the node.
   */
  get displayType() {
    // Consider all non-element nodes as displayed.
    if (InspectorActorUtils.isNodeDead(this) ||
        this.rawNode.nodeType !== Node.ELEMENT_NODE ||
        isAfterPseudoElement(this.rawNode) ||
        isBeforePseudoElement(this.rawNode)) {
      return null;
    }

    const style = this.computedStyle;
    if (!style) {
      return null;
    }

    let display = null;
    try {
      display = style.display;
    } catch (e) {
      // Fails for <scrollbar> elements.
    }

    if (SUBGRID_ENABLED &&
        (display === "grid" || display === "inline-grid") &&
        (style.gridTemplateRows === "subgrid" ||
         style.gridTemplateColumns === "subgrid")) {
      display = "subgrid";
    }

    return display;
  },

  /**
   * Is the node currently displayed?
   */
  get isDisplayed() {
    const type = this.displayType;

    // Consider all non-elements or elements with no display-types to be displayed.
    if (!type) {
      return true;
    }

    // Otherwise consider elements to be displayed only if their display-types is other
    // than "none"".
    return type !== "none";
  },

  /**
   * Are there event listeners that are listening on this node? This method
   * uses all parsers registered via event-parsers.js.registerEventParser() to
   * check if there are any event listeners.
   */
  get _hasEventListeners() {
    const parsers = this._eventParsers;
    for (const [, {hasListeners}] of parsers) {
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

  writeAttrs: function() {
    if (!this.rawNode.attributes) {
      return undefined;
    }

    return [...this.rawNode.attributes].map(attr => {
      return {namespace: attr.namespace, name: attr.name, value: attr.value };
    });
  },

  writePseudoClassLocks: function() {
    if (this.rawNode.nodeType !== Node.ELEMENT_NODE) {
      return undefined;
    }
    let ret = undefined;
    for (const pseudo of PSEUDO_CLASSES) {
      if (InspectorUtils.hasPseudoClassLock(this.rawNode, pseudo)) {
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
  getEventListeners: function(node) {
    const parsers = this._eventParsers;
    const dbg = this.parent().targetActor.makeDebugger();
    const listenerArray = [];

    for (const [, {getListeners, normalizeListener}] of parsers) {
      try {
        const listeners = getListeners(node);

        if (!listeners) {
          continue;
        }

        for (const listener of listeners) {
          if (normalizeListener) {
            listener.normalizeListener = normalizeListener;
          }

          this.processHandlerForEvent(node, listenerArray, dbg, listener);
        }
      } catch (e) {
        // An object attached to the node looked like a listener but wasn't...
        // do nothing.
      }
    }

    listenerArray.sort((a, b) => {
      return a.type.localeCompare(b.type);
    });

    return listenerArray;
  },

  /**
   * Process a handler
   *
   * @param  {Node} node
   *         The node for which we want information.
   * @param  {Array} listenerArray
   *         listenerArray contains all event objects that we have gathered
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
   *               DOM0: true
   *             },
   *             native: false
   *           }
   */
  processHandlerForEvent: function(node, listenerArray, dbg, listener) {
    const { handler } = listener;
    const global = Cu.getGlobalForObject(handler);
    const globalDO = dbg.addDebuggee(global);
    let listenerDO = globalDO.makeDebuggeeValue(handler);

    const { normalizeListener } = listener;

    if (normalizeListener) {
      listenerDO = normalizeListener(listenerDO, listener);
    }

    const { capturing } = listener;
    let dom0 = false;
    let functionSource = handler.toString();
    const hide = listener.hide || {};
    let line = 0;
    let native = false;
    const override = listener.override || {};
    const tags = listener.tags || "";
    const type = listener.type || "";
    let url = "";

    // If the listener is an object with a 'handleEvent' method, use that.
    if (listenerDO.class === "Object" || /^XUL\w*Element$/.test(listenerDO.class)) {
      let desc;

      while (!desc && listenerDO) {
        desc = listenerDO.getOwnPropertyDescriptor("handleEvent");
        listenerDO = listenerDO.proto;
      }

      if (desc && desc.value) {
        listenerDO = desc.value;
      }
    }

    // If the listener is bound to a different context then we need to switch
    // to the bound function.
    if (listenerDO.isBoundFunction) {
      listenerDO = listenerDO.boundTargetFunction;
    }

    const { isArrowFunction, name, script, parameterNames } = listenerDO;

    if (script) {
      const scriptSource = script.source.text;

      // Scripts are provided via script tags. If it wasn't provided by a
      // script tag it must be a DOM0 event.
      if (script.source.element) {
        dom0 = script.source.element.class !== "HTMLScriptElement";
      } else {
        dom0 = false;
      }

      line = script.startLine;
      url = script.url;

      // Checking for the string "[native code]" is the only way at this point
      // to check for native code. Even if this provides a false positive then
      // grabbing the source code a second time is harmless.
      if (functionSource === "[object Object]" ||
          functionSource === "[object XULElement]" ||
          functionSource.includes("[native code]")) {
        functionSource =
          scriptSource.substr(script.sourceStart, script.sourceLength);

        // At this point the script looks like this:
        // () { ... }
        // We prefix this with "function" if it is not a fat arrow function.
        if (!isArrowFunction) {
          functionSource = "function " + functionSource;
        }
      }
    } else {
      // If the listener is a native one (provided by C++ code) then we have no
      // access to the script. We use the native flag to prevent showing the
      // debugger button because the script is not available.
      native = true;
    }

    // Fat arrow function text always contains the parameters. Function
    // parameters are often missing e.g. if Array.sort is used as a handler.
    // If they are missing we provide the parameters ourselves.
    if (parameterNames && parameterNames.length > 0) {
      const prefix = "function " + name + "()";
      const paramString = parameterNames.join(", ");

      if (functionSource.startsWith(prefix)) {
        functionSource = functionSource.substr(prefix.length);

        functionSource = `function ${name} (${paramString})${functionSource}`;
      }
    }

    // If the listener is native code we display the filename "[native code]."
    // This is the official string and should *not* be translated.
    let origin;
    if (native) {
      origin = "[native code]";
    } else {
      origin = url + ((dom0 || line === 0) ? "" : ":" + line);
    }

    const eventObj = {
      type: override.type || type,
      handler: override.handler || functionSource.trim(),
      origin: override.origin || origin,
      tags: override.tags || tags,
      DOM0: typeof override.dom0 !== "undefined" ? override.dom0 : dom0,
      capturing: typeof override.capturing !== "undefined" ?
                 override.capturing : capturing,
      hide: typeof override.hide !== "undefined" ? override.hide : hide,
      native
    };

    // Hide the debugger icon for DOM0 and native listeners. DOM0 listeners are
    // generated dynamically from e.g. an onclick="" attribute so the script
    // doesn't actually exist.
    if (native || dom0) {
      eventObj.hide.debugger = true;
    }

    listenerArray.push(eventObj);

    dbg.removeDebuggee(globalDO);
  },

  /**
   * Returns a LongStringActor with the node's value.
   */
  getNodeValue: function() {
    return new LongStringActor(this.conn, this.rawNode.nodeValue || "");
  },

  /**
   * Set the node's value to a given string.
   */
  setNodeValue: function(value) {
    this.rawNode.nodeValue = value;
  },

  /**
   * Get a unique selector string for this node.
   */
  getUniqueSelector: function() {
    if (Cu.isDeadWrapper(this.rawNode)) {
      return "";
    }
    return findCssSelector(this.rawNode);
  },

  /**
   * Get the full CSS path for this node.
   *
   * @return {String} A CSS selector with a part for the node and each of its ancestors.
   */
  getCssPath: function() {
    if (Cu.isDeadWrapper(this.rawNode)) {
      return "";
    }
    return getCssPath(this.rawNode);
  },

  /**
   * Get the XPath for this node.
   *
   * @return {String} The XPath for finding this node on the page.
   */
  getXPath: function() {
    if (Cu.isDeadWrapper(this.rawNode)) {
      return "";
    }
    return getXPath(this.rawNode);
  },

  /**
   * Scroll the selected node into view.
   */
  scrollIntoView: function() {
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
  getImageData: function(maxDim) {
    return InspectorActorUtils.imageToImageData(this.rawNode, maxDim).then(imageData => {
      return {
        data: LongStringActor(this.conn, imageData.data),
        size: imageData.size
      };
    });
  },

  /**
   * Get all event listeners that are listening on this node.
   */
  getEventListenerInfo: function() {
    const node = this.rawNode;

    if (this.rawNode.nodeName.toLowerCase() === "html") {
      const winListeners = this.getEventListeners(node.ownerGlobal) || [];
      const docElementListeners = this.getEventListeners(node) || [];
      const docListeners = this.getEventListeners(node.parentNode) || [];

      return [...winListeners, ...docElementListeners, ...docListeners].sort((a, b) => {
        return a.type.localeCompare(b.type);
      });
    }
    return this.getEventListeners(node);
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
  modifyAttributes: function(modifications) {
    const rawNode = this.rawNode;
    for (const change of modifications) {
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
  getFontFamilyDataURL: function(font, fillStyle = "black") {
    const doc = this.rawNode.ownerDocument;
    const options = {
      previewText: FONT_FAMILY_PREVIEW_TEXT,
      previewFontSize: FONT_FAMILY_PREVIEW_TEXT_SIZE,
      fillStyle: fillStyle
    };
    const { dataURL, size } = getFontPreviewData(font, doc, options);

    return { data: LongStringActor(this.conn, dataURL), size: size };
  },

  /**
   * Finds the computed background color of the closest parent with
   * a set background color.
   * Returns a string with the background color of the form
   * rgba(r, g, b, a). Defaults to rgba(255, 255, 255, 1) if no
   * background color is found.
   */
  getClosestBackgroundColor: function() {
    let current = this.rawNode;
    while (current) {
      const computedStyle = CssLogic.getComputedStyle(current);
      const currentStyle = computedStyle.getPropertyValue("background-color");
      if (colorUtils.isValidCSSColor(currentStyle)) {
        const currentCssColor = new colorUtils.CssColor(currentStyle);
        if (!currentCssColor.isTransparent()) {
          return currentCssColor.rgba;
        }
      }
      current = current.parentNode;
    }
    return "rgba(255, 255, 255, 1)";
  }
});

/**
 * Server side of a node list as returned by querySelectorAll()
 */
const NodeListActor = protocol.ActorClassWithSpec(nodeListSpec, {
  typeName: "domnodelist",

  initialize: function(walker, nodeList) {
    protocol.Actor.prototype.initialize.call(this);
    this.walker = walker;
    this.nodeList = nodeList || [];
  },

  destroy: function() {
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
  marshallPool: function() {
    return this.walker;
  },

  // Returns the JSON representation of this object over the wire.
  form: function() {
    return {
      actor: this.actorID,
      length: this.nodeList ? this.nodeList.length : 0
    };
  },

  /**
   * Get a single node from the node list.
   */
  item: function(index) {
    return this.walker.attachElement(this.nodeList[index]);
  },

  /**
   * Get a range of the items from the node list.
   */
  items: function(start = 0, end = this.nodeList.length) {
    const items = Array.prototype.slice.call(this.nodeList, start, end)
      .map(item => this.walker._ref(item));
    return this.walker.attachElements(items);
  },

  release: function() {}
});

exports.NodeActor = NodeActor;
exports.NodeListActor = NodeListActor;
