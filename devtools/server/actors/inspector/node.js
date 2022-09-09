/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const InspectorUtils = require("InspectorUtils");
const protocol = require("devtools/shared/protocol");
const { PSEUDO_CLASSES } = require("devtools/shared/css/constants");
const { nodeSpec, nodeListSpec } = require("devtools/shared/specs/node");

loader.lazyRequireGetter(
  this,
  ["getCssPath", "getXPath", "findCssSelector"],
  "devtools/shared/inspector/css-logic",
  true
);

loader.lazyRequireGetter(
  this,
  [
    "getShadowRootMode",
    "isAfterPseudoElement",
    "isAnonymous",
    "isBeforePseudoElement",
    "isDirectShadowHostChild",
    "isFrameBlockedByCSP",
    "isFrameWithChildTarget",
    "isMarkerPseudoElement",
    "isNativeAnonymous",
    "isShadowHost",
    "isShadowRoot",
  ],
  "devtools/shared/layout/utils",
  true
);

loader.lazyRequireGetter(
  this,
  [
    "getBackgroundColor",
    "getClosestBackgroundColor",
    "getNodeDisplayName",
    "imageToImageData",
    "isNodeDead",
  ],
  "devtools/server/actors/inspector/utils",
  true
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
  "devtools/server/actors/utils/style-utils",
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
  "DOMHelpers",
  "devtools/shared/dom-helpers",
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
  initialize(walker, node) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.walker = walker;
    this.rawNode = node;
    this._eventCollector = new EventCollector(this.walker.targetActor);
    // Map<id -> nsIEventListenerInfo> that we maintain to be able to disable/re-enable event listeners
    // The id is generated from getEventListenerInfo
    this._nsIEventListenersInfo = new Map();

    // Store the original display type and scrollable state and whether or not the node is
    // displayed to track changes when reflows occur.
    const wasScrollable = this.isScrollable;

    this.currentDisplayType = this.displayType;
    this.wasDisplayed = this.isDisplayed;
    this.wasScrollable = wasScrollable;

    if (wasScrollable) {
      this.walker.updateOverflowCausingElements(
        this,
        this.walker.overflowCausingElementsMap
      );
    }
  },

  toString() {
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

  isDocumentElement() {
    return (
      this.rawNode.ownerDocument &&
      this.rawNode.ownerDocument.documentElement === this.rawNode
    );
  },

  destroy() {
    protocol.Actor.prototype.destroy.call(this);

    if (this.mutationObserver) {
      if (!Cu.isDeadWrapper(this.mutationObserver)) {
        this.mutationObserver.disconnect();
      }
      this.mutationObserver = null;
    }

    if (this.slotchangeListener) {
      if (!isNodeDead(this)) {
        this.rawNode.removeEventListener("slotchange", this.slotchangeListener);
      }
      this.slotchangeListener = null;
    }

    if (this._waitForFrameLoadAbortController) {
      this._waitForFrameLoadAbortController.abort();
      this._waitForFrameLoadAbortController = null;
    }
    if (this._waitForFrameLoadIntervalId) {
      clearInterval(this._waitForFrameLoadIntervalId);
      this._waitForFrameLoadIntervalId = null;
    }

    if (this._nsIEventListenersInfo) {
      // Re-enable all event listeners that we might have disabled
      for (const nsIEventListenerInfo of this._nsIEventListenersInfo.values()) {
        // If event listeners/node don't exist anymore, accessing nsIEventListenerInfo.enabled
        // will throw.
        try {
          if (!nsIEventListenerInfo.enabled) {
            nsIEventListenerInfo.enabled = true;
          }
        } catch (e) {
          // ignore
        }
      }
      this._nsIEventListenersInfo = null;
    }

    this._eventCollector.destroy();
    this._eventCollector = null;
    this.rawNode = null;
    this.walker = null;
  },

  // Returns the JSON representation of this object over the wire.
  form() {
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
      displayName: getNodeDisplayName(this.rawNode),
      numChildren: this.numChildren,
      inlineTextChild: inlineTextChild ? inlineTextChild.form() : undefined,
      displayType: this.displayType,
      isScrollable: this.isScrollable,
      isTopLevelDocument: this.isTopLevelDocument,
      causesOverflow: this.walker.overflowCausingElementsMap.has(this.rawNode),

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
      traits: {},
    };

    if (this.isDocumentElement()) {
      form.isDocumentElement = true;
    }

    if (isFrameBlockedByCSP(this.rawNode)) {
      form.numChildren = 0;
    }

    // Flag the node if a different walker is needed to retrieve its children (i.e. if
    // this is a remote frame, or if it's an iframe and we're creating targets for every iframes)
    if (this.useChildTargetToFetchChildren) {
      form.useChildTargetToFetchChildren = true;
      // Declare at least one child (the #document element) so
      // that they can be expanded.
      form.numChildren = 1;
    }
    form.browsingContextID = this.rawNode.browsingContext?.id;

    return form;
  },

  /**
   * Watch the given document node for mutations using the DOM observer
   * API.
   */
  watchDocument(doc, callback) {
    if (!doc.defaultView) {
      return;
    }

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
  watchSlotchange(callback) {
    this.slotchangeListener = callback;
    this.rawNode.addEventListener("slotchange", this.slotchangeListener);
  },

  /**
   * Check if the current node represents an element (e.g. an iframe) which has a dedicated
   * target for its underlying document that we would need to use to fetch the child nodes.
   * This will be the case for iframes if EFT is enabled, or if this is a remote iframe and
   * fission is enabled.
   */
  get useChildTargetToFetchChildren() {
    return isFrameWithChildTarget(this.walker.targetActor, this.rawNode);
  },

  get isTopLevelDocument() {
    return this.rawNode === this.walker.rootDoc;
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
    const hasContentDocument = rawNode.contentDocument;
    const hasSVGDocument = rawNode.getSVGDocument && rawNode.getSVGDocument();
    if (numChildren === 0 && (hasContentDocument || hasSVGDocument)) {
      // This might be an iframe with virtual children.
      numChildren = 1;
    }

    // Normal counting misses ::before/::after.  Also, some anonymous children
    // may ultimately be skipped, so we have to consult with the walker.
    //
    // FIXME: We should be able to just check <slot> rather than
    // containingShadowRoot.
    if (
      numChildren === 0 ||
      isShadowHost(this.rawNode) ||
      this.rawNode.containingShadowRoot
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
    if (isNodeDead(this) || this.rawNode.nodeType !== Node.ELEMENT_NODE) {
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
      (style.gridTemplateRows.startsWith("subgrid") ||
        style.gridTemplateColumns.startsWith("subgrid"))
    ) {
      display = "subgrid";
    }

    return display;
  },

  /**
   * Check whether the node currently has scrollbars and is scrollable.
   */
  get isScrollable() {
    return (
      this.rawNode.nodeType === Node.ELEMENT_NODE &&
      this.rawNode.hasVisibleScrollbars
    );
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
    const dbg = this.getParent().targetActor.makeDebugger();
    return this._eventCollector.hasEventListeners(this.rawNode, dbg);
  },

  writeAttrs() {
    // If the node has no attributes or this.rawNode is the document node and a
    // node with `name="attributes"` exists in the DOM we need to bail.
    if (
      !this.rawNode.attributes ||
      !NamedNodeMap.isInstance(this.rawNode.attributes)
    ) {
      return undefined;
    }

    return [...this.rawNode.attributes].map(attr => {
      return { namespace: attr.namespace, name: attr.name, value: attr.value };
    });
  },

  writePseudoClassLocks() {
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
   * Retrieve the script location of the custom element definition for this node, when
   * relevant. To be linked to a custom element definition
   */
  getCustomElementLocation() {
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

    const dbg = this.getParent().targetActor.makeDebugger();

    // If we hit a <browser> element of Firefox, its global will be the chrome window
    // which is system principal and will be in the same compartment as the debuggee.
    // For some reason, this happens when we run the content toolbox. As for the content
    // toolboxes, the modules are loaded in the same compartment as the <browser> element,
    // this throws as the debugger can _not_ be in the same compartment as the debugger.
    // This happens when we toggle fission for content toolbox because we try to reparent
    // the Walker of the tab. This happens because we do not detect in Walker.reparentRemoteFrame
    // that the target of the tab is the top level. That's because the target is a WindowGlobalTargetActor
    // which is retrieved via Node.getEmbedderElement and doesn't return the LocalTabTargetActor.
    // We should probably work on TabDescriptor so that the LocalTabTargetActor has a descriptor,
    // and see if we can possibly move the local tab specific out of the TargetActor and have
    // the TabDescriptor expose a pure WindowGlobalTargetActor?? (See bug 1579042)
    if (Cu.getObjectPrincipal(global) == Cu.getObjectPrincipal(dbg)) {
      return undefined;
    }

    const globalDO = dbg.addDebuggee(global);
    const customElementDO = globalDO.makeDebuggeeValue(customElement);

    // Return undefined if we can't find a script for the custom element definition.
    if (!customElementDO.script) {
      return undefined;
    }

    return {
      url: customElementDO.script.url,
      line: customElementDO.script.startLine,
      column: customElementDO.script.startColumn,
    };
  },

  /**
   * Returns a LongStringActor with the node's value.
   */
  getNodeValue() {
    return new LongStringActor(this.conn, this.rawNode.nodeValue || "");
  },

  /**
   * Set the node's value to a given string.
   */
  setNodeValue(value) {
    this.rawNode.nodeValue = value;
  },

  /**
   * Get a unique selector string for this node.
   */
  getUniqueSelector() {
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
  getCssPath() {
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
  getXPath() {
    if (Cu.isDeadWrapper(this.rawNode)) {
      return "";
    }
    return getXPath(this.rawNode);
  },

  /**
   * Scroll the selected node into view.
   */
  scrollIntoView() {
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
  getImageData(maxDim) {
    return imageToImageData(this.rawNode, maxDim).then(imageData => {
      return {
        data: LongStringActor(this.conn, imageData.data),
        size: imageData.size,
      };
    });
  },

  /**
   * Get all event listeners that are listening on this node.
   */
  getEventListenerInfo() {
    this._nsIEventListenersInfo.clear();

    const eventListenersData = this._eventCollector.getEventListeners(
      this.rawNode
    );
    let counter = 0;
    for (const eventListenerData of eventListenersData) {
      if (eventListenerData.nsIEventListenerInfo) {
        const id = `event-listener-info-${++counter}`;
        this._nsIEventListenersInfo.set(
          id,
          eventListenerData.nsIEventListenerInfo
        );

        eventListenerData.eventListenerInfoId = id;
        // remove the nsIEventListenerInfo since we don't want to send it to the client.
        delete eventListenerData.nsIEventListenerInfo;
      }
    }
    return eventListenersData;
  },

  /**
   * Disable a specific event listener given its associated id
   *
   * @param {String} eventListenerInfoId
   */
  disableEventListener(eventListenerInfoId) {
    const nsEventListenerInfo = this._nsIEventListenersInfo.get(
      eventListenerInfoId
    );
    if (!nsEventListenerInfo) {
      throw new Error("Unkown nsEventListenerInfo");
    }
    nsEventListenerInfo.enabled = false;
  },

  /**
   * (Re-)enable a specific event listener given its associated id
   *
   * @param {String} eventListenerInfoId
   */
  enableEventListener(eventListenerInfoId) {
    const nsEventListenerInfo = this._nsIEventListenersInfo.get(
      eventListenerInfoId
    );
    if (!nsEventListenerInfo) {
      throw new Error("Unkown nsEventListenerInfo");
    }
    nsEventListenerInfo.enabled = true;
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
  modifyAttributes(modifications) {
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
        rawNode.setAttributeDevtoolsNS(
          change.attributeNamespace,
          change.attributeName,
          change.newValue
        );
      } else {
        rawNode.setAttributeDevtools(change.attributeName, change.newValue);
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
  getFontFamilyDataURL(font, fillStyle = "black") {
    const doc = this.rawNode.ownerDocument;
    const options = {
      previewText: FONT_FAMILY_PREVIEW_TEXT,
      previewFontSize: FONT_FAMILY_PREVIEW_TEXT_SIZE,
      fillStyle,
    };
    const { dataURL, size } = getFontPreviewData(font, doc, options);

    return { data: LongStringActor(this.conn, dataURL), size };
  },

  /**
   * Finds the computed background color of the closest parent with a set background
   * color.
   *
   * @return {String}
   *         String with the background color of the form rgba(r, g, b, a). Defaults to
   *         rgba(255, 255, 255, 1) if no background color is found.
   */
  getClosestBackgroundColor() {
    return getClosestBackgroundColor(this.rawNode);
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
  getBackgroundColor() {
    return getBackgroundColor(this);
  },

  /**
   * Returns an object with the width and height of the node's owner window.
   *
   * @return {Object}
   */
  getOwnerGlobalDimensions() {
    const win = this.rawNode.ownerGlobal;
    return {
      innerWidth: win.innerWidth,
      innerHeight: win.innerHeight,
    };
  },

  /**
   * If the current node is an iframe, wait for the content window to be loaded.
   */
  async waitForFrameLoad() {
    if (this.useChildTargetToFetchChildren) {
      // If the document is handled by a dedicated target, we'll wait for a DOCUMENT_EVENT
      // on the created target.
      throw new Error(
        "iframe content document has its own target, use that one instead"
      );
    }

    if (Cu.isDeadWrapper(this.rawNode)) {
      throw new Error("Node is dead");
    }

    const { contentDocument } = this.rawNode;
    if (!contentDocument) {
      throw new Error("Can't access contentDocument");
    }

    if (contentDocument.readyState === "uninitialized") {
      // If the readyState is "uninitialized", the document is probably an about:blank
      // transient document. In such case, we want to wait until the "final" document
      // is inserted.

      const { chromeEventHandler } = this.rawNode.ownerGlobal.docShell;
      const browsingContextID = this.rawNode.browsingContext.id;
      await new Promise((resolve, reject) => {
        this._waitForFrameLoadAbortController = new AbortController();

        chromeEventHandler.addEventListener(
          "DOMDocElementInserted",
          e => {
            const { browsingContext } = e.target.defaultView;
            // Check that the document we're notified about is the iframe one.
            if (browsingContext.id == browsingContextID) {
              resolve();
              this._waitForFrameLoadAbortController.abort();
            }
          },
          { signal: this._waitForFrameLoadAbortController.signal }
        );

        // It might happen that the "final" document will be a remote one, living in a
        // different process, which means we won't get the DOMDocElementInserted event
        // here, and will wait forever. To prevent this Promise to hang forever, we use
        // a setInterval to check if the final document can be reached, so we can reject
        // if it's not.
        // This is definitely not a perfect solution, but I wasn't able to find something
        // better for this feature. I think it's _fine_ as this method will be removed
        // when EFT is  enabled everywhere in release.
        this._waitForFrameLoadIntervalId = setInterval(() => {
          if (Cu.isDeadWrapper(this.rawNode) || !this.rawNode.contentDocument) {
            reject("Can't access the iframe content document");
            clearInterval(this._waitForFrameLoadIntervalId);
            this._waitForFrameLoadIntervalId = null;
            this._waitForFrameLoadAbortController.abort();
          }
        }, 50);
      });
    }

    if (this.rawNode.contentDocument.readyState === "loading") {
      await new Promise(resolve => {
        DOMHelpers.onceDOMReady(this.rawNode.contentWindow, resolve);
      });
    }
  },
});

/**
 * Server side of a node list as returned by querySelectorAll()
 */
const NodeListActor = protocol.ActorClassWithSpec(nodeListSpec, {
  initialize(walker, nodeList) {
    protocol.Actor.prototype.initialize.call(this);
    this.walker = walker;
    this.nodeList = nodeList || [];
  },

  destroy() {
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
  marshallPool() {
    return this.walker;
  },

  // Returns the JSON representation of this object over the wire.
  form() {
    return {
      actor: this.actorID,
      length: this.nodeList ? this.nodeList.length : 0,
    };
  },

  /**
   * Get a single node from the node list.
   */
  item(index) {
    return this.walker.attachElement(this.nodeList[index]);
  },

  /**
   * Get a range of the items from the node list.
   */
  items(start = 0, end = this.nodeList.length) {
    const items = Array.prototype.slice
      .call(this.nodeList, start, end)
      .map(item => this.walker._getOrCreateNodeActor(item));
    return this.walker.attachElements(items);
  },

  release() {},
});

exports.NodeActor = NodeActor;
exports.NodeListActor = NodeListActor;
