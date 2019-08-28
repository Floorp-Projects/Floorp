/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const Services = require("Services");
const ChromeUtils = require("ChromeUtils");
const InspectorUtils = require("InspectorUtils");
const protocol = require("devtools/shared/protocol");
const { PSEUDO_CLASSES } = require("devtools/shared/css/constants");
const { nodeSpec, nodeListSpec } = require("devtools/shared/specs/node");
const {
  connectToFrame,
} = require("devtools/server/connectors/frame-connector");

loader.lazyRequireGetter(
  this,
  "getCssPath",
  "devtools/shared/inspector/css-logic",
  true
);
loader.lazyRequireGetter(
  this,
  "getXPath",
  "devtools/shared/inspector/css-logic",
  true
);
loader.lazyRequireGetter(
  this,
  "findCssSelector",
  "devtools/shared/inspector/css-logic",
  true
);

loader.lazyRequireGetter(
  this,
  "isAfterPseudoElement",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isAnonymous",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isBeforePseudoElement",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isDirectShadowHostChild",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isMarkerPseudoElement",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isNativeAnonymous",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isShadowAnonymous",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isShadowHost",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isShadowRoot",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "getShadowRootMode",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isXBLAnonymous",
  "devtools/shared/layout/utils",
  true
);

loader.lazyRequireGetter(
  this,
  "InspectorActorUtils",
  "devtools/server/actors/inspector/utils"
);
loader.lazyRequireGetter(
  this,
  "LongStringActor",
  "devtools/server/actors/string",
  true
);
loader.lazyRequireGetter(
  this,
  "getFontPreviewData",
  "devtools/server/actors/styles",
  true
);
loader.lazyRequireGetter(
  this,
  "CssLogic",
  "devtools/server/actors/inspector/css-logic",
  true
);
loader.lazyRequireGetter(
  this,
  "EventCollector",
  "devtools/server/actors/inspector/event-collector",
  true
);
loader.lazyRequireGetter(
  this,
  "DocumentWalker",
  "devtools/server/actors/inspector/document-walker",
  true
);
loader.lazyRequireGetter(
  this,
  "scrollbarTreeWalkerFilter",
  "devtools/server/actors/inspector/utils",
  true
);

const SUBGRID_ENABLED = Services.prefs.getBoolPref(
  "layout.css.grid-template-subgrid-value.enabled"
);

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
    this._eventCollector = new EventCollector(this.walker.targetActor);

    // Store the original display type and scrollable state and whether or not the node is
    // displayed to track changes when reflows occur.
    this.currentDisplayType = this.displayType;
    this.wasDisplayed = this.isDisplayed;
    this.wasScrollable = this.isScrollable;
  },

  toString: function() {
    return (
      "[NodeActor " + this.actorID + " for " + this.rawNode.toString() + "]"
    );
  },

  /**
   * Instead of storing a connection object, the NodeActor gets its connection
   * from its associated walker.
   */
  get conn() {
    return this.walker.conn;
  },

  isDocumentElement: function() {
    return (
      this.rawNode.ownerDocument &&
      this.rawNode.ownerDocument.documentElement === this.rawNode
    );
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

    this._eventCollector.destroy();
    this._eventCollector = null;
    this.rawNode = null;
    this.walker = null;
  },

  // Returns the JSON representation of this object over the wire.
  form: function() {
    const parentNode = this.walker.parentNode(this);
    const inlineTextChild = this.walker.inlineTextChild(this);
    const shadowRoot = isShadowRoot(this.rawNode);
    const hostActor = shadowRoot
      ? this.walker.getNode(this.rawNode.host)
      : null;

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
      isScrollable: this.isScrollable,

      // doctype attributes
      name: this.rawNode.name,
      publicId: this.rawNode.publicId,
      systemId: this.rawNode.systemId,

      attrs: this.writeAttrs(),
      customElementLocation: this.getCustomElementLocation(),
      isMarkerPseudoElement: isMarkerPseudoElement(this.rawNode),
      isBeforePseudoElement: isBeforePseudoElement(this.rawNode),
      isAfterPseudoElement: isAfterPseudoElement(this.rawNode),
      isAnonymous: isAnonymous(this.rawNode),
      isNativeAnonymous: isNativeAnonymous(this.rawNode),
      isXBLAnonymous: isXBLAnonymous(this.rawNode),
      isShadowAnonymous: isShadowAnonymous(this.rawNode),
      isShadowRoot: shadowRoot,
      shadowRootMode: getShadowRootMode(this.rawNode),
      isShadowHost: isShadowHost(this.rawNode),
      isDirectShadowHostChild: isDirectShadowHostChild(this.rawNode),
      pseudoClassLocks: this.writePseudoClassLocks(),
      mutationBreakpoints: this.walker.getMutationBreakpoints(this),

      isDisplayed: this.isDisplayed,
      isInHTMLDocument:
        this.rawNode.ownerDocument &&
        this.rawNode.ownerDocument.contentType === "text/html",
      hasEventListeners: this._hasEventListeners,
    };

    if (this.isDocumentElement()) {
      form.isDocumentElement = true;
    }

    // Flag the remote frame and declare at least one child (the #document element) so
    // that they can be expanded.
    if (this.isRemoteFrame) {
      form.remoteFrame = true;
      form.numChildren = 1;
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
      subtree: true,
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

  /**
   * Check if the current node is representing a remote frame.
   * EXPERIMENTAL: Only works if fission is enabled in the toolbox.
   */
  get isRemoteFrame() {
    return (
      this.numChildren == 0 &&
      ChromeUtils.getClassName(this.rawNode) == "XULFrameElement" &&
      this.rawNode.getAttribute("remote") == "true"
    );
  },

  // Estimate the number of children that the walker will return without making
  // a call to children() if possible.
  get numChildren() {
    // For pseudo elements, childNodes.length returns 1, but the walker
    // will return 0.
    if (
      isMarkerPseudoElement(this.rawNode) ||
      isBeforePseudoElement(this.rawNode) ||
      isAfterPseudoElement(this.rawNode)
    ) {
      return 0;
    }

    const rawNode = this.rawNode;
    let numChildren = rawNode.childNodes.length;
    const hasAnonChildren =
      rawNode.nodeType === Node.ELEMENT_NODE &&
      rawNode.ownerDocument.getAnonymousNodes(rawNode);

    const hasContentDocument = rawNode.contentDocument;
    const hasSVGDocument = rawNode.getSVGDocument && rawNode.getSVGDocument();
    if (numChildren === 0 && (hasContentDocument || hasSVGDocument)) {
      // This might be an iframe with virtual children.
      numChildren = 1;
    }

    // Normal counting misses ::before/::after.  Also, some anonymous children
    // may ultimately be skipped, so we have to consult with the walker.
    if (
      numChildren === 0 ||
      hasAnonChildren ||
      isShadowHost(this.rawNode) ||
      isShadowAnonymous(this.rawNode)
    ) {
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
    if (
      InspectorActorUtils.isNodeDead(this) ||
      this.rawNode.nodeType !== Node.ELEMENT_NODE
    ) {
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

    if (
      SUBGRID_ENABLED &&
      (display === "grid" || display === "inline-grid") &&
      (style.gridTemplateRows === "subgrid" ||
        style.gridTemplateColumns === "subgrid")
    ) {
      display = "subgrid";
    }

    return display;
  },

  /**
   * Check whether the node currently has scrollbars and is scrollable.
   */
  get isScrollable() {
    // Check first if the element has an overflow area, bail out if not.
    if (
      this.rawNode.clientHeight === this.rawNode.scrollHeight &&
      this.rawNode.clientWidth === this.rawNode.scrollWidth
    ) {
      return false;
    }

    // If it does, then check it also has scrollbars.
    try {
      const walker = new DocumentWalker(
        this.rawNode,
        this.rawNode.ownerGlobal,
        { filter: scrollbarTreeWalkerFilter }
      );
      return !!walker.firstChild();
    } catch (e) {
      // We have no access to a DOM object. This is probably due to a CORS
      // violation. Using try / catch is the only way to avoid this error.
      return false;
    }
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
    // We need to pass a debugger instance from this compartment because
    // otherwise we can't make use of it inside the event-collector module.
    const dbg = this.parent().targetActor.makeDebugger();
    return this._eventCollector.hasEventListeners(this.rawNode, dbg);
  },

  writeAttrs: function() {
    // If the node has no attributes or this.rawNode is the document node and a
    // node with `name="attributes"` exists in the DOM we need to bail.
    if (
      !this.rawNode.attributes ||
      !(this.rawNode.attributes instanceof NamedNodeMap)
    ) {
      return undefined;
    }

    return [...this.rawNode.attributes].map(attr => {
      return { namespace: attr.namespace, name: attr.name, value: attr.value };
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
    return this._eventCollector.getEventListeners(node);
  },

  /**
   * Retrieve the script location of the custom element definition for this node, when
   * relevant. To be linked to a custom element definition
   */
  getCustomElementLocation: function() {
    // Get a reference to the custom element definition function.
    const name = this.rawNode.localName;

    if (!this.rawNode.ownerGlobal) {
      return undefined;
    }

    const customElementsRegistry = this.rawNode.ownerGlobal.customElements;
    const customElement =
      customElementsRegistry && customElementsRegistry.get(name);
    if (!customElement) {
      return undefined;
    }
    // Create debugger object for the customElement function.
    const global = Cu.getGlobalForObject(customElement);
    const dbg = this.parent().targetActor.makeDebugger();
    const globalDO = dbg.addDebuggee(global);
    const customElementDO = globalDO.makeDebuggeeValue(customElement);

    // Return undefined if we can't find a script for the custom element definition.
    if (!customElementDO.script) {
      return undefined;
    }

    return {
      url: customElementDO.script.url,
      line: customElementDO.script.startLine,
    };
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
    return InspectorActorUtils.imageToImageData(this.rawNode, maxDim).then(
      imageData => {
        return {
          data: LongStringActor(this.conn, imageData.data),
          size: imageData.size,
        };
      }
    );
  },

  /**
   * Get all event listeners that are listening on this node.
   */
  getEventListenerInfo: function() {
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
  modifyAttributes: function(modifications) {
    const rawNode = this.rawNode;
    for (const change of modifications) {
      if (change.newValue == null) {
        if (change.attributeNamespace) {
          rawNode.removeAttributeNS(
            change.attributeNamespace,
            change.attributeName
          );
        } else {
          rawNode.removeAttribute(change.attributeName);
        }
      } else if (change.attributeNamespace) {
        rawNode.setAttributeNS(
          change.attributeNamespace,
          change.attributeName,
          change.newValue
        );
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
      fillStyle: fillStyle,
    };
    const { dataURL, size } = getFontPreviewData(font, doc, options);

    return { data: LongStringActor(this.conn, dataURL), size: size };
  },

  /**
   * Finds the computed background color of the closest parent with a set background
   * color.
   *
   * @return {String}
   *         String with the background color of the form rgba(r, g, b, a). Defaults to
   *         rgba(255, 255, 255, 1) if no background color is found.
   */
  getClosestBackgroundColor: function() {
    return InspectorActorUtils.getClosestBackgroundColor(this.rawNode);
  },

  /**
   * Finds the background color range for the parent of a single text node
   * (i.e. for multi-colored backgrounds with gradients, images) or a single
   * background color for single-colored backgrounds. Defaults to the closest
   * background color if an error is encountered.
   *
   * @return {Object}
   *         Object with one or more of the following properties: value, min, max
   */
  getBackgroundColor: function() {
    return InspectorActorUtils.getBackgroundColor(this);
  },

  /**
   * Returns an object with the width and height of the node's owner window.
   *
   * @return {Object}
   */
  getOwnerGlobalDimensions: function() {
    const win = this.rawNode.ownerGlobal;
    return {
      innerWidth: win.innerWidth,
      innerHeight: win.innerHeight,
    };
  },

  /**
   * Fetch the target actor's form for the current remote frame.
   *
   * (to be called only if form.remoteFrame is true)
   */
  connectToRemoteFrame() {
    if (!this.isRemoteFrame) {
      return {
        error: "ErrorRemoteFrame",
        message: "Tried to call `connectToRemoteFrame` on a local frame",
      };
    }
    return connectToFrame(this.conn, this.rawNode);
  },
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
      length: this.nodeList ? this.nodeList.length : 0,
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
    const items = Array.prototype.slice
      .call(this.nodeList, start, end)
      .map(item => this.walker._ref(item));
    return this.walker.attachElements(items);
  },

  release: function() {},
});

exports.NodeActor = NodeActor;
exports.NodeListActor = NodeListActor;
