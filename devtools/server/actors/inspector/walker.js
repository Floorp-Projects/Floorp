/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");

const Services = require("Services");
const protocol = require("devtools/shared/protocol");
const { walkerSpec } = require("devtools/shared/specs/walker");
const { LongStringActor } = require("devtools/server/actors/string");
const InspectorUtils = require("InspectorUtils");
const {
  EXCLUDED_LISTENER,
} = require("devtools/server/actors/inspector/constants");

loader.lazyRequireGetter(
  this,
  [
    "getFrameElement",
    "isAfterPseudoElement",
    "isAnonymous",
    "isBeforePseudoElement",
    "isDirectShadowHostChild",
    "isMarkerPseudoElement",
    "isNativeAnonymous",
    "isFrameWithChildTarget",
    "isShadowHost",
    "isShadowRoot",
    "isTemplateElement",
    "loadSheet",
  ],
  "devtools/shared/layout/utils",
  true
);

loader.lazyRequireGetter(this, "throttle", "devtools/shared/throttle", true);

loader.lazyRequireGetter(
  this,
  [
    "allAnonymousContentTreeWalkerFilter",
    "findGridParentContainerForNode",
    "isDocumentReady",
    "isNodeDead",
    "noAnonymousContentTreeWalkerFilter",
    "nodeDocument",
    "standardTreeWalkerFilter",
  ],
  "devtools/server/actors/inspector/utils",
  true
);

loader.lazyRequireGetter(
  this,
  "CustomElementWatcher",
  "devtools/server/actors/inspector/custom-element-watcher",
  true
);
loader.lazyRequireGetter(
  this,
  ["DocumentWalker", "SKIP_TO_SIBLING"],
  "devtools/server/actors/inspector/document-walker",
  true
);
loader.lazyRequireGetter(
  this,
  ["NodeActor", "NodeListActor"],
  "devtools/server/actors/inspector/node",
  true
);
loader.lazyRequireGetter(
  this,
  "NodePicker",
  "devtools/server/actors/inspector/node-picker",
  true
);
loader.lazyRequireGetter(
  this,
  "LayoutActor",
  "devtools/server/actors/layout",
  true
);
loader.lazyRequireGetter(
  this,
  ["getLayoutChangesObserver", "releaseLayoutChangesObserver"],
  "devtools/server/actors/reflow",
  true
);
loader.lazyRequireGetter(
  this,
  "WalkerSearch",
  "devtools/server/actors/utils/walker-search",
  true
);
loader.lazyRequireGetter(
  this,
  "hasStyleSheetWatcherSupportForTarget",
  "devtools/server/actors/utils/stylesheets-manager",
  true
);

// ContentDOMReference requires ChromeUtils, which isn't available in worker context.
if (!isWorker) {
  loader.lazyRequireGetter(
    this,
    "ContentDOMReference",
    "resource://gre/modules/ContentDOMReference.jsm",
    true
  );
}

loader.lazyServiceGetter(
  this,
  "eventListenerService",
  "@mozilla.org/eventlistenerservice;1",
  "nsIEventListenerService"
);

// Minimum delay between two "new-mutations" events.
const MUTATIONS_THROTTLING_DELAY = 100;
// List of mutation types that should -not- be throttled.
const IMMEDIATE_MUTATIONS = ["pseudoClassLock"];

const HIDDEN_CLASS = "__fx-devtools-hide-shortcut__";

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
  ["::selection", 0],
  ["::marker", 0],
];

const HELPER_SHEET =
  "data:text/css;charset=utf-8," +
  encodeURIComponent(`
  .__fx-devtools-hide-shortcut__ {
    visibility: hidden !important;
  }
`);

/**
 * We only send nodeValue up to a certain size by default.  This stuff
 * controls that size.
 */
exports.DEFAULT_VALUE_SUMMARY_LENGTH = 50;
var gValueSummaryLength = exports.DEFAULT_VALUE_SUMMARY_LENGTH;

exports.getValueSummaryLength = function() {
  return gValueSummaryLength;
};

exports.setValueSummaryLength = function(val) {
  gValueSummaryLength = val;
};

/**
 * Server side of the DOM walker.
 */
var WalkerActor = protocol.ActorClassWithSpec(walkerSpec, {
  /**
   * Create the WalkerActor
   * @param {DevToolsServerConnection} conn
   *        The server connection.
   * @param {TargetActor} targetActor
   *        The top-level Actor for this tab.
   * @param {Object} options
   *        - {Boolean} showAllAnonymousContent: Show all native anonymous content
   */
  initialize: function(conn, targetActor, options) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
    this.rootWin = targetActor.window;
    this.rootDoc = this.rootWin.document;

    // Map of already created node actors, keyed by their corresponding DOMNode.
    this._nodeActorsMap = new Map();

    this._pendingMutations = [];
    this._activePseudoClassLocks = new Set();
    this._mutationBreakpoints = new WeakMap();
    this.customElementWatcher = new CustomElementWatcher(
      targetActor.chromeEventHandler
    );

    // In this map, the key-value pairs are the overflow causing elements and their
    // respective ancestor scrollable node actor.
    this.overflowCausingElementsMap = new Map();

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

    this.onSubtreeModified = this.onSubtreeModified.bind(this);
    this.onSubtreeModified[EXCLUDED_LISTENER] = true;
    this.onNodeRemoved = this.onNodeRemoved.bind(this);
    this.onNodeRemoved[EXCLUDED_LISTENER] = true;
    this.onAttributeModified = this.onAttributeModified.bind(this);
    this.onAttributeModified[EXCLUDED_LISTENER] = true;

    this.onMutations = this.onMutations.bind(this);
    this.onSlotchange = this.onSlotchange.bind(this);
    this.onShadowrootattached = this.onShadowrootattached.bind(this);
    this.onFrameLoad = this.onFrameLoad.bind(this);
    this.onFrameUnload = this.onFrameUnload.bind(this);
    this.onCustomElementDefined = this.onCustomElementDefined.bind(this);
    this._throttledEmitNewMutations = throttle(
      this._emitNewMutations.bind(this),
      MUTATIONS_THROTTLING_DELAY
    );

    targetActor.on("will-navigate", this.onFrameUnload);
    targetActor.on("window-ready", this.onFrameLoad);

    this.customElementWatcher.on(
      "element-defined",
      this.onCustomElementDefined
    );

    // Keep a reference to the chromeEventHandler for the current targetActor, to make
    // sure we will be able to remove the listener during the WalkerActor destroy().
    this.chromeEventHandler = targetActor.chromeEventHandler;
    // shadowrootattached is a chrome-only event. We enable it below.
    this.chromeEventHandler.addEventListener(
      "shadowrootattached",
      this.onShadowrootattached
    );

    for (const { document } of this.targetActor.windows) {
      document.shadowRootAttachedEventEnabled = true;
    }

    // Ensure that the root document node actor is ready and
    // managed.
    this.rootNode = this.document();

    this.layoutChangeObserver = getLayoutChangesObserver(this.targetActor);
    this._onReflows = this._onReflows.bind(this);
    this.layoutChangeObserver.on("reflows", this._onReflows);
    this._onResize = this._onResize.bind(this);
    this.layoutChangeObserver.on("resize", this._onResize);

    this._onEventListenerChange = this._onEventListenerChange.bind(this);
    eventListenerService.addListenerChangeListener(this._onEventListenerChange);
  },

  get nodePicker() {
    if (!this._nodePicker) {
      this._nodePicker = new NodePicker(this, this.targetActor);
    }

    return this._nodePicker;
  },

  watchRootNode() {
    if (this.rootNode && isDocumentReady(this.rootDoc)) {
      this.emit("root-available", this.rootNode);
    }
  },

  /**
   * Callback for eventListenerService.addListenerChangeListener
   * @param nsISimpleEnumerator changesEnum
   *    enumerator of nsIEventListenerChange
   */
  _onEventListenerChange: function(changesEnum) {
    for (const current of changesEnum.enumerate(Ci.nsIEventListenerChange)) {
      const target = current.target;

      if (this._nodeActorsMap.has(target)) {
        const actor = this.getNode(target);
        const mutation = {
          type: "events",
          target: actor.actorID,
          hasEventListeners: actor._hasEventListeners,
        };
        this.queueMutation(mutation);
      }
    }
  },

  // Returns the JSON representation of this object over the wire.
  form: function() {
    return {
      actor: this.actorID,
      root: this.rootNode.form(),
      traits: {},
    };
  },

  toString: function() {
    return "[WalkerActor " + this.actorID + "]";
  },

  getAnonymousDocumentWalker: function(node, whatToShow, skipTo) {
    // Allow native anon content (like <video> controls) if preffed on
    const filter = this.showAllAnonymousContent
      ? allAnonymousContentTreeWalkerFilter
      : standardTreeWalkerFilter;

    return new DocumentWalker(node, this.rootWin, {
      whatToShow,
      filter,
      skipTo,
      showAnonymousContent: true,
    });
  },

  getNonAnonymousDocumentWalker: function(node, whatToShow, skipTo) {
    const nodeFilter = standardTreeWalkerFilter;

    return new DocumentWalker(node, this.rootWin, {
      whatToShow,
      nodeFilter,
      skipTo,
      showAnonymousContent: false,
    });
  },

  /**
   * Will first try to create a regular anonymous document walker. If it fails, will fall
   * back on a non-anonymous walker.
   */
  getDocumentWalker: function(node, whatToShow, skipTo) {
    try {
      return this.getAnonymousDocumentWalker(node, whatToShow, skipTo);
    } catch (e) {
      return this.getNonAnonymousDocumentWalker(node, whatToShow, skipTo);
    }
  },

  destroy: function() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;
    protocol.Actor.prototype.destroy.call(this);
    try {
      this.clearPseudoClassLocks();
      this._activePseudoClassLocks = null;

      this.overflowCausingElementsMap.clear();
      this.overflowCausingElementsMap = null;

      this._hoveredNode = null;
      this.rootWin = null;
      this.rootDoc = null;
      this.rootNode = null;
      this.layoutHelpers = null;
      this._orphaned = null;
      this._retainedOrphans = null;
      this._nodeActorsMap = null;

      this.targetActor.off("will-navigate", this.onFrameUnload);
      this.targetActor.off("window-ready", this.onFrameLoad);
      this.customElementWatcher.off(
        "element-defined",
        this.onCustomElementDefined
      );

      this.chromeEventHandler.removeEventListener(
        "shadowrootattached",
        this.onShadowrootattached
      );

      // This event is just for devtools, so we can unset once we're done.
      for (const { document } of this.targetActor.windows) {
        document.shadowRootAttachedEventEnabled = false;
      }

      this.onFrameLoad = null;
      this.onFrameUnload = null;

      this.customElementWatcher.destroy();
      this.customElementWatcher = null;

      this.walkerSearch.destroy();

      if (this._nodePicker) {
        this._nodePicker.destroy();
        this._nodePicker = null;
      }

      this.layoutChangeObserver.off("reflows", this._onReflows);
      this.layoutChangeObserver.off("resize", this._onResize);
      this.layoutChangeObserver = null;
      releaseLayoutChangesObserver(this.targetActor);

      eventListenerService.removeListenerChangeListener(
        this._onEventListenerChange
      );

      this.onMutations = null;

      this.layoutActor = null;
      this.targetActor = null;
      this.chromeEventHandler = null;

      this.emit("destroyed");
    } catch (e) {
      console.error(e);
    }
  },

  release: function() {},

  unmanage: function(actor) {
    if (actor instanceof NodeActor) {
      if (
        this._activePseudoClassLocks &&
        this._activePseudoClassLocks.has(actor)
      ) {
        this.clearPseudoClassLocks(actor);
      }

      this.customElementWatcher.unmanageNode(actor);

      this._nodeActorsMap.delete(actor.rawNode);
    }
    protocol.Actor.prototype.unmanage.call(this, actor);
  },

  /**
   * Determine if the walker has come across this DOM node before.
   * @param {DOMNode} rawNode
   * @return {Boolean}
   */
  hasNode: function(rawNode) {
    return this._nodeActorsMap.has(rawNode);
  },

  /**
   * If the walker has come across this DOM node before, then get the
   * corresponding node actor.
   * @param {DOMNode} rawNode
   * @return {NodeActor}
   */
  getNode: function(rawNode) {
    return this._nodeActorsMap.get(rawNode);
  },

  /**
   * Internal helper that will either retrieve the existing NodeActor for the
   * provided node or create the actor on the fly if it doesn't exist.
   * This method should only be called when we are sure that the node should be
   * known by the client and that the parent node is already known.
   *
   * Otherwise prefer `getNode` to only retrieve known actors or `attachElement`
   * to create node actors recursively.
   *
   * @param  {DOMNode} node
   *         The node for which we want to create or get an actor
   * @return {NodeActor} The corresponding NodeActor
   */
  _getOrCreateNodeActor: function(node) {
    let actor = this.getNode(node);
    if (actor) {
      return actor;
    }

    actor = new NodeActor(this, node);

    // Add the node actor as a child of this walker actor, assigning
    // it an actorID.
    this.manage(actor);
    this._nodeActorsMap.set(node, actor);

    if (node.nodeType === Node.DOCUMENT_NODE) {
      actor.watchDocument(node, this.onMutations);
    }

    if (isShadowRoot(actor.rawNode)) {
      actor.watchDocument(node.ownerDocument, this.onMutations);
      actor.watchSlotchange(this.onSlotchange);
    }

    this.customElementWatcher.manageNode(actor);

    return actor;
  },

  /**
   * When a custom element is defined, send a customElementDefined mutation for all the
   * NodeActors using this tag name.
   */
  onCustomElementDefined: function({ name, actors }) {
    actors.forEach(actor =>
      this.queueMutation({
        target: actor.actorID,
        type: "customElementDefined",
        customElementLocation: actor.getCustomElementLocation(),
      })
    );
  },

  _onReflows: function(reflows) {
    // Going through the nodes the walker knows about, see which ones have had their
    // display, scrollable or overflow state changed and send events if any.
    const displayTypeChanges = [];
    const scrollableStateChanges = [];

    const currentOverflowCausingElementsMap = new Map();

    for (const [node, actor] of this._nodeActorsMap) {
      if (Cu.isDeadWrapper(node)) {
        continue;
      }

      const displayType = actor.displayType;
      const isDisplayed = actor.isDisplayed;

      if (
        displayType !== actor.currentDisplayType ||
        isDisplayed !== actor.wasDisplayed
      ) {
        displayTypeChanges.push(actor);

        // Updating the original value
        actor.currentDisplayType = displayType;
        actor.wasDisplayed = isDisplayed;
      }

      const isScrollable = actor.isScrollable;
      if (isScrollable !== actor.wasScrollable) {
        scrollableStateChanges.push(actor);
        actor.wasScrollable = isScrollable;
      }

      if (isScrollable) {
        this.updateOverflowCausingElements(
          actor,
          currentOverflowCausingElementsMap
        );
      }
    }

    // Get the NodeActor for each node in the symmetric difference of
    // currentOverflowCausingElementsMap and this.overflowCausingElementsMap
    const overflowStateChanges = [...currentOverflowCausingElementsMap.keys()]
      .filter(node => !this.overflowCausingElementsMap.has(node))
      .concat(
        [...this.overflowCausingElementsMap.keys()].filter(
          node => !currentOverflowCausingElementsMap.has(node)
        )
      )
      .filter(node => this.hasNode(node))
      .map(node => this.getNode(node));

    this.overflowCausingElementsMap = currentOverflowCausingElementsMap;

    if (overflowStateChanges.length) {
      this.emit("overflow-change", overflowStateChanges);
    }

    if (displayTypeChanges.length) {
      this.emit("display-change", displayTypeChanges);
    }

    if (scrollableStateChanges.length) {
      this.emit("scrollable-change", scrollableStateChanges);
    }
  },

  /**
   * When the browser window gets resized, relay the event to the front.
   */
  _onResize: function() {
    this.emit("resize");
  },

  /**
   * Ensures that the node is attached and it can be accessed from the root.
   *
   * @param {(Node|NodeActor)} nodes The nodes
   * @return {Object} An object compatible with the disconnectedNode type.
   */
  attachElement: function(node) {
    const { nodes, newParents } = this.attachElements([node]);
    return {
      node: nodes[0],
      newParents: newParents,
    };
  },

  /**
   * Ensures that the nodes are attached and they can be accessed from the root.
   *
   * @param {(Node[]|NodeActor[])} nodes The nodes
   * @return {Object} An object compatible with the disconnectedNodeArray type.
   */
  attachElements: function(nodes) {
    const nodeActors = [];
    const newParents = new Set();
    for (let node of nodes) {
      if (!(node instanceof NodeActor)) {
        // If an anonymous node was passed in and we aren't supposed to know
        // about it, then consult with the document walker as the source of
        // truth about which elements exist.
        if (!this.showAllAnonymousContent && isAnonymous(node)) {
          node = this.getDocumentWalker(node).currentNode;
        }

        node = this._getOrCreateNodeActor(node);
      }

      this.ensurePathToRoot(node, newParents);
      // If nodes may be an array of raw nodes, we're sure to only have
      // NodeActors with the following array.
      nodeActors.push(node);
    }

    return {
      nodes: nodeActors,
      newParents: [...newParents],
    };
  },

  /**
   * Return the document node that contains the given node,
   * or the root node if no node is specified.
   * @param NodeActor node
   *        The node whose document is needed, or null to
   *        return the root.
   */
  document: function(node) {
    const doc = isNodeDead(node) ? this.rootDoc : nodeDocument(node.rawNode);
    return this._getOrCreateNodeActor(doc);
  },

  /**
   * Return the documentElement for the document containing the
   * given node.
   * @param NodeActor node
   *        The node whose documentElement is requested, or null
   *        to use the root document.
   */
  documentElement: function(node) {
    const elt = isNodeDead(node)
      ? this.rootDoc.documentElement
      : nodeDocument(node.rawNode).documentElement;
    return this._getOrCreateNodeActor(elt);
  },

  parentNode: function(node) {
    const parent = this.rawParentNode(node);
    if (parent) {
      return this._getOrCreateNodeActor(parent);
    }

    return null;
  },

  rawParentNode: function(node) {
    let parent;
    try {
      // If the node is the child of a shadow host, we can not use an anonymous walker to
      // get the shadow host parent.
      const walker = isDirectShadowHostChild(node.rawNode)
        ? this.getNonAnonymousDocumentWalker(node.rawNode)
        : this.getAnonymousDocumentWalker(node.rawNode);
      parent = walker.parentNode();
    } catch (e) {
      // When getting the parent node for a child of a non-slotted shadow host child,
      // walker.parentNode() will throw if the walker is anonymous, because non-slotted
      // shadow host children are not accessible anywhere in the anonymous tree.
      const walker = this.getNonAnonymousDocumentWalker(node.rawNode);
      parent = walker.parentNode();
    }

    return parent;
  },

  /**
   * If the given NodeActor only has a single text node as a child with a text
   * content small enough to be inlined, return that child's NodeActor.
   *
   * @param NodeActor node
   */
  inlineTextChild: function({ rawNode }) {
    // Quick checks to prevent creating a new walker if possible.
    if (
      isMarkerPseudoElement(rawNode) ||
      isBeforePseudoElement(rawNode) ||
      isAfterPseudoElement(rawNode) ||
      isShadowHost(rawNode) ||
      rawNode.nodeType != Node.ELEMENT_NODE ||
      rawNode.children.length > 0 ||
      isFrameWithChildTarget(this.targetActor, rawNode)
    ) {
      return undefined;
    }

    const walker = this.getDocumentWalker(rawNode);
    const firstChild = walker.firstChild();

    // Bail out if:
    // - more than one child
    // - unique child is not a text node
    // - unique child is a text node, but is too long to be inlined
    // - we are a slot -> these are always represented on their own lines with
    //                    a link to the original node.
    // - we are a flex item -> these are always shown on their own lines so they can be
    //                         selected by the flexbox inspector.
    const isAssignedSlot =
      !!firstChild &&
      rawNode.nodeName === "SLOT" &&
      isDirectShadowHostChild(firstChild);

    const isFlexItem = !!firstChild?.parentFlexElement;

    if (
      !firstChild ||
      walker.nextSibling() ||
      firstChild.nodeType !== Node.TEXT_NODE ||
      firstChild.nodeValue.length > gValueSummaryLength ||
      isAssignedSlot ||
      isFlexItem
    ) {
      return undefined;
    }

    return this._getOrCreateNodeActor(firstChild);
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
  retainNode: function(node) {
    node.retained = true;
  },

  /**
   * Remove the 'retained' mark from a node.  If the node was a
   * retained orphan, release it.
   */
  unretainNode: function(node) {
    node.retained = false;
    if (this._retainedOrphans.has(node)) {
      this._retainedOrphans.delete(node);
      this.releaseNode(node);
    }
  },

  /**
   * Release actors for a node and all child nodes.
   */
  releaseNode: function(node, options = {}) {
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

    const walker = this.getDocumentWalker(node.rawNode);

    let child = walker.firstChild();
    while (child) {
      const childActor = this.getNode(child);
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
  ensurePathToRoot: function(node, newParents = new Set()) {
    if (!node) {
      return newParents;
    }
    let parent = this.rawParentNode(node);
    while (parent) {
      let parentActor = this.getNode(parent);
      if (parentActor) {
        // This parent did exist, so the client knows about it.
        return newParents;
      }
      // This parent didn't exist, so hasn't been seen by the client yet.
      parentActor = this._getOrCreateNodeActor(parent);
      newParents.add(parentActor);
      parent = this.rawParentNode(parentActor);
    }
    return newParents;
  },

  /**
   * Return the number of children under the provided NodeActor.
   *
   * @param NodeActor node
   *    See JSDoc for children()
   * @param object options
   *    See JSDoc for children()
   * @return Number the number of children
   */
  countChildren: function(node, options = {}) {
    return this._getChildren(node, options).nodes.length;
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
   *    nodes: Array of NodeActor representing the nodes returned by the request.
   */
  children: function(node, options = {}) {
    const { hasFirst, hasLast, nodes } = this._getChildren(node, options);
    return {
      hasFirst,
      hasLast,
      nodes: nodes.map(n => this._getOrCreateNodeActor(n)),
    };
  },

  /**
   * Return chidlren of the given node. Contrary to children children(), this method only
   * returns DOMNodes. Therefore it will not create NodeActor wrappers and will not
   * update the nodeActors map for the discovered nodes either. This makes this method
   * safe to call when you are not sure if the discovered nodes will be communicated to
   * the client.
   *
   * @param NodeActor node
   *    See JSDoc for children()
   * @param object options
   *    See JSDoc for children()
   * @return  an object with three items:
   *    hasFirst: true if the first child of the node is included in the list.
   *    hasLast: true if the last child of the node is included in the list.
   *    nodes: Array of DOMNodes.
   */
  // eslint-disable-next-line complexity
  _getChildren: function(node, options = {}) {
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

    const directShadowHostChild = isDirectShadowHostChild(node.rawNode);
    const shadowHost = isShadowHost(node.rawNode);
    const shadowRoot = isShadowRoot(node.rawNode);

    // UA Widgets are internal Firefox widgets such as videocontrols implemented
    // using shadow DOM. By default, their shadow root should be hidden for web
    // developers.
    const isUAWidget =
      shadowHost && node.rawNode.openOrClosedShadowRoot.isUAWidget();
    const hideShadowRoot = isUAWidget && !this.showAllAnonymousContent;
    const showNativeAnonymousChildren =
      isUAWidget && this.showAllAnonymousContent;

    const templateElement = isTemplateElement(node.rawNode);
    if (templateElement) {
      // <template> tags should have a single child pointing to the element's template
      // content.
      const documentFragment = node.rawNode.content;
      const nodes = [documentFragment];
      return { hasFirst: true, hasLast: true, nodes };
    }

    // Detect special case of unslotted shadow host children that cannot rely on a
    // regular anonymous walker.
    let isUnslottedHostChild = false;
    if (directShadowHostChild) {
      try {
        this.getDocumentWalker(
          node.rawNode,
          options.whatToShow,
          SKIP_TO_SIBLING
        );
      } catch (e) {
        isUnslottedHostChild = true;
      }
    }

    const useNonAnonymousWalker =
      shadowRoot || shadowHost || isUnslottedHostChild;

    // We're going to create a few document walkers with the same filter,
    // make it easier.
    const getFilteredWalker = documentWalkerNode => {
      const { whatToShow } = options;

      // Use SKIP_TO_SIBLING to force the walker to use a sibling of the provided node
      // in case this one is incompatible with the walker's filter function.
      const skipTo = SKIP_TO_SIBLING;

      if (useNonAnonymousWalker) {
        // Do not use an anonymous walker for :
        // - shadow roots: if the host element has an ::after pseudo element, a walker on
        //   the last child of the shadow root will jump to the ::after element, which is
        //   not a child of the shadow root.
        //   TODO: For this case, should rather use an anonymous walker with a new
        //         dedicated filter.
        // - shadow hosts: anonymous children of host elements make up the shadow dom,
        //   while we want to return the direct children of the shadow host.
        // - unslotted host child: if a shadow host child is not slotted, it is not part
        //   of any anonymous tree and cannot be used with anonymous tree walkers.
        return this.getNonAnonymousDocumentWalker(
          documentWalkerNode,
          whatToShow,
          skipTo
        );
      }
      return this.getDocumentWalker(documentWalkerNode, whatToShow, skipTo);
    };

    // Need to know the first and last child.
    const rawNode = node.rawNode;
    const firstChild = getFilteredWalker(rawNode).firstChild();
    const lastChild = getFilteredWalker(rawNode).lastChild();

    if (!firstChild && !shadowHost) {
      // No children, we're done.
      return { hasFirst: true, hasLast: true, nodes: [] };
    }

    let nodes = [];

    if (firstChild) {
      let start;
      if (options.center) {
        start = options.center.rawNode;
      } else if (options.start) {
        start = options.start.rawNode;
      } else {
        start = firstChild;
      }

      // If we are using a non anonymous walker, we cannot start on:
      // - a shadow root
      // - a native anonymous node
      if (
        useNonAnonymousWalker &&
        (isShadowRoot(start) || isNativeAnonymous(start))
      ) {
        start = firstChild;
      }

      // Start by reading backward from the starting point if we're centering...
      const backwardWalker = getFilteredWalker(start);
      if (backwardWalker.currentNode != firstChild && options.center) {
        backwardWalker.previousSibling();
        const backwardCount = Math.floor(maxNodes / 2);
        const backwardNodes = this._readBackward(backwardWalker, backwardCount);
        nodes = backwardNodes;
      }

      // Then read forward by any slack left in the max children...
      const forwardWalker = getFilteredWalker(start);
      const forwardCount = maxNodes - nodes.length;
      nodes = nodes.concat(this._readForward(forwardWalker, forwardCount));

      // If there's any room left, it means we've run all the way to the end.
      // If we're centering, check if there are more items to read at the front.
      const remaining = maxNodes - nodes.length;
      if (options.center && remaining > 0 && nodes[0].rawNode != firstChild) {
        const firstNodes = this._readBackward(backwardWalker, remaining);

        // Then put it all back together.
        nodes = firstNodes.concat(nodes);
      }
    }

    let hasFirst, hasLast;
    if (nodes.length > 0) {
      // Compare first/last with expected nodes before modifying the nodes array in case
      // this is a shadow host.
      hasFirst = nodes[0] == firstChild;
      hasLast = nodes[nodes.length - 1] == lastChild;
    } else {
      // If nodes is still an empty array, we are on a host element with a shadow root but
      // no direct children.
      hasFirst = hasLast = true;
    }

    if (shadowHost) {
      // Use anonymous walkers to fetch ::marker / ::before / ::after pseudo
      // elements
      const firstChildWalker = this.getDocumentWalker(node.rawNode);
      const first = firstChildWalker.firstChild();
      const hasMarker =
        first && first.nodeName === "_moz_generated_content_marker";
      const maybeBeforeNode = hasMarker
        ? firstChildWalker.nextSibling()
        : first;
      const hasBefore =
        maybeBeforeNode &&
        maybeBeforeNode.nodeName === "_moz_generated_content_before";

      const lastChildWalker = this.getDocumentWalker(node.rawNode);
      const last = lastChildWalker.lastChild();
      const hasAfter = last && last.nodeName === "_moz_generated_content_after";

      nodes = [
        // #shadow-root
        ...(hideShadowRoot ? [] : [node.rawNode.openOrClosedShadowRoot]),
        // ::marker
        ...(hasMarker ? [first] : []),
        // ::before
        ...(hasBefore ? [maybeBeforeNode] : []),
        // shadow host direct children
        ...nodes,
        // native anonymous content for UA widgets
        ...(showNativeAnonymousChildren
          ? this.getNativeAnonymousChildren(node.rawNode)
          : []),
        // ::after
        ...(hasAfter ? [last] : []),
      ];
    }

    return { hasFirst, hasLast, nodes };
  },

  getNativeAnonymousChildren: function(rawNode) {
    // Get an anonymous walker and start on the first child.
    const walker = this.getDocumentWalker(rawNode);
    let node = walker.firstChild();

    const nodes = [];
    while (node) {
      // We only want native anonymous content here.
      if (isNativeAnonymous(node)) {
        nodes.push(node);
      }
      node = walker.nextSibling();
    }
    return nodes;
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
  nextSibling: function(node, options = {}) {
    if (isNodeDead(node)) {
      return null;
    }

    const walker = this.getDocumentWalker(node.rawNode, options.whatToShow);
    const sibling = walker.nextSibling();
    return sibling ? this._getOrCreateNodeActor(sibling) : null;
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
  previousSibling: function(node, options = {}) {
    if (isNodeDead(node)) {
      return null;
    }

    const walker = this.getDocumentWalker(node.rawNode, options.whatToShow);
    const sibling = walker.previousSibling();
    return sibling ? this._getOrCreateNodeActor(sibling) : null;
  },

  /**
   * Helper function for the `children` method: Read forward in the sibling
   * list into an array with `count` items, including the current node.
   */
  _readForward: function(walker, count) {
    const ret = [];

    let node = walker.currentNode;
    do {
      if (!walker.isSkippedNode(node)) {
        // The walker can be on a node that would be filtered out if it didn't find any
        // other node to fallback to.
        ret.push(node);
      }
      node = walker.nextSibling();
    } while (node && --count);
    return ret;
  },

  /**
   * Helper function for the `children` method: Read backward in the sibling
   * list into an array with `count` items, including the current node.
   */
  _readBackward: function(walker, count) {
    const ret = [];

    let node = walker.currentNode;
    do {
      if (!walker.isSkippedNode(node)) {
        // The walker can be on a node that would be filtered out if it didn't find any
        // other node to fallback to.
        ret.push(node);
      }
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
  querySelector: function(baseNode, selector) {
    if (isNodeDead(baseNode)) {
      return {};
    }

    const node = baseNode.rawNode.querySelector(selector);
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
  querySelectorAll: function(baseNode, selector) {
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
  _multiFrameQuerySelectorAll: function(selector) {
    let nodes = [];

    for (const { document } of this.targetActor.windows) {
      try {
        nodes = [...nodes, ...document.querySelectorAll(selector)];
      } catch (e) {
        // Bad selector. Do nothing as the selector can come from a searchbox.
      }
    }

    return nodes;
  },

  /**
   * Get a list of nodes that match the given XPath in all known frames of
   * the current content page.
   * @param {String} xPath.
   * @return {Array}
   */
  _multiFrameXPath: function(xPath) {
    const nodes = [];

    for (const window of this.targetActor.windows) {
      const document = window.document;
      try {
        const result = document.evaluate(
          xPath,
          document.documentElement,
          null,
          window.XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,
          null
        );

        for (let i = 0; i < result.snapshotLength; i++) {
          nodes.push(result.snapshotItem(i));
        }
      } catch (e) {
        // Bad XPath. Do nothing as the XPath can come from a searchbox.
      }
    }

    return nodes;
  },

  /**
   * Return a NodeListActor with all nodes that match the given XPath in all
   * frames of the current content page.
   * @param {String} xPath
   */
  multiFrameXPath: function(xPath) {
    return new NodeListActor(this, this._multiFrameXPath(xPath));
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
  search: function(query) {
    const results = this.walkerSearch.search(query);
    const nodeList = new NodeListActor(
      this,
      results.map(r => r.node)
    );

    return {
      list: nodeList,
      metadata: [],
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
  // eslint-disable-next-line complexity
  getSuggestionsForQuery: function(query, completing, selectorState) {
    const sugs = {
      classes: new Map(),
      tags: new Map(),
      ids: new Map(),
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
        for (const node of nodes) {
          for (const className of node.classList) {
            sugs.classes.set(className, (sugs.classes.get(className) | 0) + 1);
          }
        }
        sugs.classes.delete("");
        sugs.classes.delete(HIDDEN_CLASS);
        for (const [className, count] of sugs.classes) {
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
        for (const node of nodes) {
          sugs.ids.set(node.id, (sugs.ids.get(node.id) | 0) + 1);
        }
        for (const [id, count] of sugs.ids) {
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
        for (const node of nodes) {
          const tag = node.localName;
          sugs.tags.set(tag, (sugs.tags.get(tag) | 0) + 1);
        }
        for (const [tag, count] of sugs.tags) {
          if (new RegExp("^" + completing + ".*", "i").test(tag)) {
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
            ...this.getSuggestionsForQuery(null, completing, "id").suggestions,
          ];
        }

        break;

      case "null":
        nodes = this._multiFrameQuerySelectorAll(query);
        for (const node of nodes) {
          sugs.ids.set(node.id, (sugs.ids.get(node.id) | 0) + 1);
          const tag = node.localName;
          sugs.tags.set(tag, (sugs.tags.get(tag) | 0) + 1);
          for (const className of node.classList) {
            sugs.classes.set(className, (sugs.classes.get(className) | 0) + 1);
          }
        }
        for (const [tag, count] of sugs.tags) {
          tag && result.push([tag, count]);
        }
        for (const [id, count] of sugs.ids) {
          id && result.push(["#" + id, count]);
        }
        sugs.classes.delete("");
        sugs.classes.delete(HIDDEN_CLASS);
        for (const [className, count] of sugs.classes) {
          className && result.push(["." + className, count]);
        }
    }

    // Sort by count (desc) and name (asc)
    result = result.sort((a, b) => {
      // Computed a sortable string with first the inverted count, then the name
      let sortA = 10000 - a[1] + a[0];
      let sortB = 10000 - b[1] + b[0];

      // Prefixing ids, classes and tags, to group results
      const firstA = a[0].substring(0, 1);
      const firstB = b[0].substring(0, 1);

      const getSortKeyPrefix = firstLetter => {
        if (firstLetter === "#") {
          return "2";
        }
        if (firstLetter === ".") {
          return "1";
        }
        return "0";
      };

      sortA = getSortKeyPrefix(firstA) + sortA;
      sortB = getSortKeyPrefix(firstB) + sortB;

      // String compare
      return sortA.localeCompare(sortB);
    });

    result = result.slice(0, 25);

    return {
      query: query,
      suggestions: result,
    };
  },

  /**
   * Add a pseudo-class lock to a node.
   *
   * @param NodeActor node
   * @param string pseudo
   *    A pseudoclass: ':hover', ':active', ':focus', ':focus-within'
   * @param options
   *    Options object:
   *    `parents`: True if the pseudo-class should be added
   *      to parent nodes.
   *    `enabled`: False if the pseudo-class should be locked
   *      to 'off'. Defaults to true.
   *
   * @returns An empty packet.  A "pseudoClassLock" mutation will
   *    be queued for any changed nodes.
   */
  addPseudoClassLock: function(node, pseudo, options = {}) {
    if (isNodeDead(node)) {
      return;
    }

    // There can be only one node locked per pseudo, so dismiss all existing
    // ones
    for (const locked of this._activePseudoClassLocks) {
      if (InspectorUtils.hasPseudoClassLock(locked.rawNode, pseudo)) {
        this._removePseudoClassLock(locked, pseudo);
      }
    }

    const enabled = options.enabled === undefined || options.enabled;
    this._addPseudoClassLock(node, pseudo, enabled);

    if (!options.parents) {
      return;
    }

    const walker = this.getDocumentWalker(node.rawNode);
    let cur;
    while ((cur = walker.parentNode())) {
      const curNode = this._getOrCreateNodeActor(cur);
      this._addPseudoClassLock(curNode, pseudo, enabled);
    }
  },

  _queuePseudoClassMutation: function(node) {
    this.queueMutation({
      target: node.actorID,
      type: "pseudoClassLock",
      pseudoClassLocks: node.writePseudoClassLocks(),
    });
  },

  _addPseudoClassLock: function(node, pseudo, enabled) {
    if (node.rawNode.nodeType !== Node.ELEMENT_NODE) {
      return false;
    }
    InspectorUtils.addPseudoClassLock(node.rawNode, pseudo, enabled);
    this._activePseudoClassLocks.add(node);
    this._queuePseudoClassMutation(node);
    return true;
  },

  hideNode: function(node) {
    if (isNodeDead(node)) {
      return;
    }

    loadSheet(node.rawNode.ownerGlobal, HELPER_SHEET);
    node.rawNode.classList.add(HIDDEN_CLASS);
  },

  unhideNode: function(node) {
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
   *    A pseudoclass: ':hover', ':active', ':focus', ':focus-within'
   * @param options
   *    Options object:
   *    `parents`: True if the pseudo-class should be removed
   *      from parent nodes.
   *
   * @returns An empty response.  "pseudoClassLock" mutations
   *    will be emitted for any changed nodes.
   */
  removePseudoClassLock: function(node, pseudo, options = {}) {
    if (isNodeDead(node)) {
      return;
    }

    this._removePseudoClassLock(node, pseudo);

    // Remove pseudo class for children as we don't want to allow
    // turning it on for some childs without setting it on some parents
    for (const locked of this._activePseudoClassLocks) {
      if (
        node.rawNode.contains(locked.rawNode) &&
        InspectorUtils.hasPseudoClassLock(locked.rawNode, pseudo)
      ) {
        this._removePseudoClassLock(locked, pseudo);
      }
    }

    if (!options.parents) {
      return;
    }

    const walker = this.getDocumentWalker(node.rawNode);
    let cur;
    while ((cur = walker.parentNode())) {
      const curNode = this._getOrCreateNodeActor(cur);
      this._removePseudoClassLock(curNode, pseudo);
    }
  },

  _removePseudoClassLock: function(node, pseudo) {
    if (node.rawNode.nodeType != Node.ELEMENT_NODE) {
      return false;
    }
    InspectorUtils.removePseudoClassLock(node.rawNode, pseudo);
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
  clearPseudoClassLocks: function(node) {
    if (node && isNodeDead(node)) {
      return;
    }

    if (node) {
      InspectorUtils.clearPseudoClassLocks(node.rawNode);
      this._activePseudoClassLocks.delete(node);
      this._queuePseudoClassMutation(node);
    } else {
      for (const locked of this._activePseudoClassLocks) {
        InspectorUtils.clearPseudoClassLocks(locked.rawNode);
        this._activePseudoClassLocks.delete(locked);
        this._queuePseudoClassMutation(locked);
      }
    }
  },

  /**
   * Get a node's innerHTML property.
   */
  innerHTML: function(node) {
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
  setInnerHTML: function(node, value) {
    if (isNodeDead(node)) {
      return;
    }

    const rawNode = node.rawNode;
    if (
      rawNode.nodeType !== rawNode.ownerDocument.ELEMENT_NODE &&
      rawNode.nodeType !== rawNode.ownerDocument.DOCUMENT_FRAGMENT_NODE
    ) {
      throw new Error("Can only change innerHTML to element or fragment nodes");
    }
    // eslint-disable-next-line no-unsanitized/property
    rawNode.innerHTML = value;
  },

  /**
   * Get a node's outerHTML property.
   *
   * @param {NodeActor} node The node.
   */
  outerHTML: function(node) {
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
  setOuterHTML: function(node, value) {
    if (isNodeDead(node)) {
      return;
    }

    const rawNode = node.rawNode;
    const doc = nodeDocument(rawNode);
    const win = doc.defaultView;
    let parser;
    if (!win) {
      throw new Error("The window object shouldn't be null");
    } else {
      // We create DOMParser under window object because we want a content
      // DOMParser, which means all the DOM objects created by this DOMParser
      // will be in the same DocGroup as rawNode.parentNode. Then the newly
      // created nodes can be adopted into rawNode.parentNode.
      parser = new win.DOMParser();
    }

    const mimeType = rawNode.tagName === "svg" ? "image/svg+xml" : "text/html";
    const parsedDOM = parser.parseFromString(value, mimeType);
    const parentNode = rawNode.parentNode;

    // Special case for head and body.  Setting document.body.outerHTML
    // creates an extra <head> tag, and document.head.outerHTML creates
    // an extra <body>.  So instead we will call replaceChild with the
    // parsed DOM, assuming that they aren't trying to set both tags at once.
    if (rawNode.tagName === "BODY") {
      if (parsedDOM.head.innerHTML === "") {
        parentNode.replaceChild(parsedDOM.body, rawNode);
      } else {
        // eslint-disable-next-line no-unsanitized/property
        rawNode.outerHTML = value;
      }
    } else if (rawNode.tagName === "HEAD") {
      if (parsedDOM.body.innerHTML === "") {
        parentNode.replaceChild(parsedDOM.head, rawNode);
      } else {
        // eslint-disable-next-line no-unsanitized/property
        rawNode.outerHTML = value;
      }
    } else if (node.isDocumentElement()) {
      // Unable to set outerHTML on the document element. Fall back by
      // setting attributes manually. Then replace all the child nodes.
      const finalAttributeModifications = [];
      const attributeModifications = {};
      for (const attribute of rawNode.attributes) {
        attributeModifications[attribute.name] = null;
      }
      for (const attribute of parsedDOM.documentElement.attributes) {
        attributeModifications[attribute.name] = attribute.value;
      }
      for (const key in attributeModifications) {
        finalAttributeModifications.push({
          attributeName: key,
          newValue: attributeModifications[key],
        });
      }
      node.modifyAttributes(finalAttributeModifications);

      rawNode.replaceChildren(...parsedDOM.firstElementChild.childNodes);
    } else {
      // eslint-disable-next-line no-unsanitized/property
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
  insertAdjacentHTML: function(node, position, value) {
    if (isNodeDead(node)) {
      return { node: [], newParents: [] };
    }

    const rawNode = node.rawNode;
    const isInsertAsSibling =
      position === "beforeBegin" || position === "afterEnd";

    // Don't insert anything adjacent to the document element.
    if (isInsertAsSibling && node.isDocumentElement()) {
      throw new Error("Can't insert adjacent element to the root.");
    }

    const rawParentNode = rawNode.parentNode;
    if (!rawParentNode && isInsertAsSibling) {
      throw new Error("Can't insert as sibling without parent node.");
    }

    // We can't use insertAdjacentHTML, because we want to return the nodes
    // being created (so the front can remove them if the user undoes
    // the change). So instead, use Range.createContextualFragment().
    const range = rawNode.ownerDocument.createRange();
    if (position === "beforeBegin" || position === "afterEnd") {
      range.selectNode(rawNode);
    } else {
      range.selectNodeContents(rawNode);
    }
    // eslint-disable-next-line no-unsanitized/method
    const docFrag = range.createContextualFragment(value);
    const newRawNodes = Array.from(docFrag.childNodes);
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
        throw new Error(
          "Invalid position value. Must be either " +
            "'beforeBegin', 'beforeEnd', 'afterBegin' or 'afterEnd'."
        );
    }

    return this.attachElements(newRawNodes);
  },

  /**
   * Duplicate a specified node
   *
   * @param {NodeActor} node The node to duplicate.
   */
  duplicateNode: function({ rawNode }) {
    const clonedNode = rawNode.cloneNode(true);
    rawNode.parentNode.insertBefore(clonedNode, rawNode.nextSibling);
  },

  /**
   * Test whether a node is a document or a document element.
   *
   * @param {NodeActor} node The node to remove.
   * @return {boolean} True if the node is a document or a document element.
   */
  isDocumentOrDocumentElementNode: function(node) {
    return (
      (node.rawNode.ownerDocument &&
        node.rawNode.ownerDocument.documentElement === this.rawNode) ||
      node.rawNode.nodeType === Node.DOCUMENT_NODE
    );
  },

  /**
   * Removes a node from its parent node.
   *
   * @param {NodeActor} node The node to remove.
   * @returns The node's nextSibling before it was removed.
   */
  removeNode: function(node) {
    if (isNodeDead(node) || this.isDocumentOrDocumentElementNode(node)) {
      throw Error("Cannot remove document, document elements or dead nodes.");
    }

    const nextSibling = this.nextSibling(node);
    node.rawNode.remove();
    // Mutation events will take care of the rest.
    return nextSibling;
  },

  /**
   * Removes an array of nodes from their parent node.
   *
   * @param {NodeActor[]} nodes The nodes to remove.
   */
  removeNodes: function(nodes) {
    // Check that all nodes are valid before processing the removals.
    for (const node of nodes) {
      if (isNodeDead(node) || this.isDocumentOrDocumentElementNode(node)) {
        throw Error("Cannot remove document, document elements or dead nodes");
      }
    }

    for (const node of nodes) {
      node.rawNode.remove();
      // Mutation events will take care of the rest.
    }
  },

  /**
   * Insert a node into the DOM.
   */
  insertBefore: function(node, parent, sibling) {
    if (
      isNodeDead(node) ||
      isNodeDead(parent) ||
      (sibling && isNodeDead(sibling))
    ) {
      return;
    }

    const rawNode = node.rawNode;
    const rawParent = parent.rawNode;
    const rawSibling = sibling ? sibling.rawNode : null;

    // Don't bother inserting a node if the document position isn't going
    // to change. This prevents needless iframes reloading and mutations.
    if (rawNode.parentNode === rawParent) {
      let currentNextSibling = this.nextSibling(node);
      currentNextSibling = currentNextSibling
        ? currentNextSibling.rawNode
        : null;

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
  editTagName: function(node, tagName) {
    if (isNodeDead(node)) {
      return null;
    }

    const oldNode = node.rawNode;

    // Create a new element with the same attributes as the current element and
    // prepare to replace the current node with it.
    let newNode;
    try {
      newNode = nodeDocument(oldNode).createElement(tagName);
    } catch (x) {
      // Failed to create a new element with that tag name, ignore the change,
      // and signal the error to the front.
      return Promise.reject(
        new Error("Could not change node's tagName to " + tagName)
      );
    }

    const attrs = oldNode.attributes;
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
   * Gets the state of the mutation breakpoint types for this actor.
   *
   * @param {NodeActor} node The node to get breakpoint info for.
   */
  getMutationBreakpoints(node) {
    let bps;
    if (!isNodeDead(node)) {
      bps = this._breakpointInfoForNode(node.rawNode);
    }

    return (
      bps || {
        subtree: false,
        removal: false,
        attribute: false,
      }
    );
  },

  /**
   * Set the state of some subset of mutation breakpoint types for this actor.
   *
   * @param {NodeActor} node The node to set breakpoint info for.
   * @param {Object} bps A subset of the breakpoints for this actor that
   *                            should be updated to new states.
   */
  setMutationBreakpoints(node, bps) {
    if (isNodeDead(node)) {
      return;
    }
    const rawNode = node.rawNode;

    if (rawNode.ownerDocument && !rawNode.ownerDocument.contains(rawNode)) {
      // We only allow watching for mutations on nodes that are attached to
      // documents. That allows us to clean up our mutation listeners when all
      // of the watched nodes have been removed from the document.
      return;
    }

    // This argument has nullable fields so we want to only update boolean
    // field values.
    const bpsForNode = Object.keys(bps).reduce((obj, bp) => {
      if (typeof bps[bp] === "boolean") {
        obj[bp] = bps[bp];
      }
      return obj;
    }, {});

    this._updateMutationBreakpointState("api", rawNode, {
      ...this.getMutationBreakpoints(node),
      ...bpsForNode,
    });
  },

  /**
   * Update the mutation breakpoint state for the given DOM node.
   *
   * @param {Node} rawNode The DOM node.
   * @param {Object} bpsForNode The state of each mutation bp type we support.
   */
  _updateMutationBreakpointState(mutationReason, rawNode, bpsForNode) {
    const rawDoc = rawNode.ownerDocument || rawNode;

    const docMutationBreakpoints = this._mutationBreakpointsForDoc(
      rawDoc,
      true /* createIfNeeded */
    );
    let originalBpsForNode = this._breakpointInfoForNode(rawNode);

    if (!bpsForNode && !originalBpsForNode) {
      return;
    }

    bpsForNode = bpsForNode || {};
    originalBpsForNode = originalBpsForNode || {};

    if (Object.values(bpsForNode).some(Boolean)) {
      docMutationBreakpoints.nodes.set(rawNode, bpsForNode);
    } else {
      docMutationBreakpoints.nodes.delete(rawNode);
    }
    if (originalBpsForNode.subtree && !bpsForNode.subtree) {
      docMutationBreakpoints.counts.subtree -= 1;
    } else if (!originalBpsForNode.subtree && bpsForNode.subtree) {
      docMutationBreakpoints.counts.subtree += 1;
    }

    if (originalBpsForNode.removal && !bpsForNode.removal) {
      docMutationBreakpoints.counts.removal -= 1;
    } else if (!originalBpsForNode.removal && bpsForNode.removal) {
      docMutationBreakpoints.counts.removal += 1;
    }

    if (originalBpsForNode.attribute && !bpsForNode.attribute) {
      docMutationBreakpoints.counts.attribute -= 1;
    } else if (!originalBpsForNode.attribute && bpsForNode.attribute) {
      docMutationBreakpoints.counts.attribute += 1;
    }

    this._updateDocumentMutationListeners(rawDoc);

    const actor = this.getNode(rawNode);
    if (actor) {
      this.queueMutation({
        target: actor.actorID,
        type: "mutationBreakpoint",
        mutationBreakpoints: this.getMutationBreakpoints(actor),
        mutationReason,
      });
    }
  },

  /**
   * Controls whether this DOM document has event listeners attached for
   * handling of DOM mutation breakpoints.
   *
   * @param {Document} rawDoc The DOM document.
   */
  _updateDocumentMutationListeners(rawDoc) {
    const docMutationBreakpoints = this._mutationBreakpointsForDoc(rawDoc);
    if (!docMutationBreakpoints) {
      rawDoc.devToolsWatchingDOMMutations = false;
      return;
    }

    const anyBreakpoint =
      docMutationBreakpoints.counts.subtree > 0 ||
      docMutationBreakpoints.counts.removal > 0 ||
      docMutationBreakpoints.counts.attribute > 0;

    rawDoc.devToolsWatchingDOMMutations = anyBreakpoint;

    if (docMutationBreakpoints.counts.subtree > 0) {
      this.chromeEventHandler.addEventListener(
        "devtoolschildinserted",
        this.onSubtreeModified,
        true /* capture */
      );
    } else {
      this.chromeEventHandler.removeEventListener(
        "devtoolschildinserted",
        this.onSubtreeModified,
        true /* capture */
      );
    }

    if (anyBreakpoint) {
      this.chromeEventHandler.addEventListener(
        "devtoolschildremoved",
        this.onNodeRemoved,
        true /* capture */
      );
    } else {
      this.chromeEventHandler.removeEventListener(
        "devtoolschildremoved",
        this.onNodeRemoved,
        true /* capture */
      );
    }

    if (docMutationBreakpoints.counts.attribute > 0) {
      this.chromeEventHandler.addEventListener(
        "devtoolsattrmodified",
        this.onAttributeModified,
        true /* capture */
      );
    } else {
      this.chromeEventHandler.removeEventListener(
        "devtoolsattrmodified",
        this.onAttributeModified,
        true /* capture */
      );
    }
  },

  _breakOnMutation: function(mutationType, targetNode, ancestorNode, action) {
    this.targetActor.threadActor.pauseForMutationBreakpoint(
      mutationType,
      targetNode,
      ancestorNode,
      action
    );
  },

  _mutationBreakpointsForDoc(rawDoc, createIfNeeded = false) {
    let docMutationBreakpoints = this._mutationBreakpoints.get(rawDoc);
    if (!docMutationBreakpoints && createIfNeeded) {
      docMutationBreakpoints = {
        counts: {
          subtree: 0,
          removal: 0,
          attribute: 0,
        },
        nodes: new Map(),
      };
      this._mutationBreakpoints.set(rawDoc, docMutationBreakpoints);
    }
    return docMutationBreakpoints;
  },

  _breakpointInfoForNode: function(target) {
    const docMutationBreakpoints = this._mutationBreakpointsForDoc(
      target.ownerDocument || target
    );
    return (
      (docMutationBreakpoints && docMutationBreakpoints.nodes.get(target)) ||
      null
    );
  },

  onNodeRemoved: function(evt) {
    const mutationBpInfo = this._breakpointInfoForNode(evt.target);
    const hasNodeRemovalEvent = mutationBpInfo?.removal;

    this._clearMutationBreakpointsFromSubtree(evt.target);
    if (hasNodeRemovalEvent) {
      this._breakOnMutation("nodeRemoved", evt.target);
    } else {
      this.onSubtreeModified(evt);
    }
  },

  onAttributeModified: function(evt) {
    const mutationBpInfo = this._breakpointInfoForNode(evt.target);
    if (mutationBpInfo?.attribute) {
      this._breakOnMutation("attributeModified", evt.target);
    }
  },

  onSubtreeModified: function(evt) {
    const action = evt.type === "devtoolschildinserted" ? "add" : "remove";
    let node = evt.target;
    while ((node = node.parentNode) !== null) {
      const mutationBpInfo = this._breakpointInfoForNode(node);
      if (mutationBpInfo?.subtree) {
        this._breakOnMutation("subtreeModified", evt.target, node, action);
        break;
      }
    }
  },

  _clearMutationBreakpointsFromSubtree: function(targetNode) {
    const targetDoc = targetNode.ownerDocument || targetNode;
    const docMutationBreakpoints = this._mutationBreakpointsForDoc(targetDoc);
    if (!docMutationBreakpoints || docMutationBreakpoints.nodes.size === 0) {
      // Bail early for performance. If the doc has no mutation BPs, there is
      // no reason to iterate through the children looking for things to detach.
      return;
    }

    // The walker is not limited to the subtree of the argument node, so we
    // need to ensure that we stop walking when we leave the subtree.
    const nextWalkerSibling = this._getNextTraversalSibling(targetNode);

    const walker = new DocumentWalker(targetNode, this.rootWin, {
      filter: noAnonymousContentTreeWalkerFilter,
      skipTo: SKIP_TO_SIBLING,
    });

    do {
      this._updateMutationBreakpointState("detach", walker.currentNode, null);
    } while (walker.nextNode() && walker.currentNode !== nextWalkerSibling);
  },

  _getNextTraversalSibling(targetNode) {
    const walker = new DocumentWalker(targetNode, this.rootWin, {
      filter: noAnonymousContentTreeWalkerFilter,
      skipTo: SKIP_TO_SIBLING,
    });

    while (!walker.nextSibling()) {
      if (!walker.parentNode()) {
        // If we try to step past the walker root, there is no next sibling.
        return null;
      }
    }
    return walker.currentNode;
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
  getMutations: function(options = {}) {
    const pending = this._pendingMutations || [];
    this._pendingMutations = [];
    this._waitingForGetMutations = false;

    if (options.cleanup) {
      for (const node of this._orphaned) {
        // Release the orphaned node.  Nodes or children that have been
        // retained will be moved to this._retainedOrphans.
        this.releaseNode(node);
      }
      this._orphaned = new Set();
    }

    return pending;
  },

  queueMutation: function(mutation) {
    if (!this.actorID || this._destroyed) {
      // We've been destroyed, don't bother queueing this mutation.
      return;
    }

    // Add the mutation to the list of mutations to be retrieved next.
    this._pendingMutations.push(mutation);

    // Bail out if we already emitted a new-mutations event and are waiting for a client
    // to retrieve them.
    if (this._waitingForGetMutations) {
      return;
    }

    if (IMMEDIATE_MUTATIONS.includes(mutation.type)) {
      this._emitNewMutations();
    } else {
      /**
       * If many mutations are fired at the same time, clients might sequentially request
       * children/siblings for updated nodes, which can be costly. By throttling the calls
       * to getMutations, duplicated mutations will be ignored.
       */
      this._throttledEmitNewMutations();
    }
  },

  _emitNewMutations: function() {
    if (!this.actorID || this._destroyed) {
      // Bail out if the actor was destroyed after throttling this call.
      return;
    }

    if (this._waitingForGetMutations || this._pendingMutations.length == 0) {
      // Bail out if we already fired the new-mutation event or if no mutations are
      // waiting to be retrieved.
      return;
    }

    this._waitingForGetMutations = true;
    this.emit("new-mutations");
  },

  /**
   * Handles mutations from the DOM mutation observer API.
   *
   * @param array[MutationRecord] mutations
   *    See https://developer.mozilla.org/en-US/docs/Web/API/MutationObserver#MutationRecord
   */
  onMutations: function(mutations) {
    // Notify any observers that want *all* mutations (even on nodes that aren't
    // referenced).  This is not sent over the protocol so can only be used by
    // scripts running in the server process.
    this.emit("any-mutation");

    for (const change of mutations) {
      const targetActor = this.getNode(change.target);
      if (!targetActor) {
        continue;
      }
      const targetNode = change.target;
      const type = change.type;
      const mutation = {
        type: type,
        target: targetActor.actorID,
      };

      if (type === "attributes") {
        mutation.attributeName = change.attributeName;
        mutation.attributeNamespace = change.attributeNamespace || undefined;
        mutation.newValue = targetNode.hasAttribute(mutation.attributeName)
          ? targetNode.getAttribute(mutation.attributeName)
          : null;
      } else if (type === "characterData") {
        mutation.newValue = targetNode.nodeValue;
        this._maybeQueueInlineTextChildMutation(change, targetNode);
      } else if (type === "childList" || type === "nativeAnonymousChildList") {
        // Get the list of removed and added actors that the client has seen
        // so that it can keep its ownership tree up to date.
        const removedActors = [];
        const addedActors = [];
        for (const removed of change.removedNodes) {
          const removedActor = this.getNode(removed);
          if (!removedActor) {
            // If the client never encountered this actor we don't need to
            // mention that it was removed.
            continue;
          }
          // While removed from the tree, nodes are saved as orphaned.
          this._orphaned.add(removedActor);
          removedActors.push(removedActor.actorID);
        }
        for (const added of change.addedNodes) {
          const addedActor = this.getNode(added);
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

        const inlineTextChild = this.inlineTextChild(targetActor);
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
  _maybeQueueInlineTextChildMutation: function(mutation) {
    const { oldValue, target } = mutation;
    const newValue = target.nodeValue;
    const limit = gValueSummaryLength;

    if (
      (oldValue.length <= limit && newValue.length <= limit) ||
      (oldValue.length > limit && newValue.length > limit)
    ) {
      // Bail out if the new & old values are both below/above the size limit.
      return;
    }

    const parentActor = this.getNode(target.parentNode);
    if (!parentActor || parentActor.rawNode.children.length > 0) {
      // If the parent node has other children, a character data mutation will
      // not change anything regarding inlining text nodes.
      return;
    }

    const inlineTextChild = this.inlineTextChild(parentActor);
    this.queueMutation({
      type: "inlineTextChild",
      target: parentActor.actorID,
      inlineTextChild: inlineTextChild ? inlineTextChild.form() : undefined,
    });
  },

  onSlotchange: function(event) {
    const target = event.target;
    const targetActor = this.getNode(target);
    if (!targetActor) {
      return;
    }

    this.queueMutation({
      type: "slotchange",
      target: targetActor.actorID,
    });
  },

  onShadowrootattached: function(event) {
    const actor = this.getNode(event.target);
    if (!actor) {
      return;
    }

    const mutation = {
      type: "shadowRootAttached",
      target: actor.actorID,
    };
    this.queueMutation(mutation);
  },

  onFrameLoad: function({ window, isTopLevel }) {
    // By the time we receive the DOMContentLoaded event, we might have been destroyed
    if (this._destroyed) {
      return;
    }
    const { readyState } = window.document;
    if (readyState != "interactive" && readyState != "complete") {
      // The document is not loaded, so we want to register to fire again when the
      // DOM has been loaded.
      window.addEventListener(
        "DOMContentLoaded",
        this.onFrameLoad.bind(this, { window, isTopLevel }),
        { once: true }
      );
      return;
    }

    window.document.shadowRootAttachedEventEnabled = true;

    if (isTopLevel) {
      // If we initialize the inspector while the document is loading,
      // we may already have a root document set in the constructor.
      if (
        this.rootDoc &&
        this.rootDoc !== window.document &&
        !Cu.isDeadWrapper(this.rootDoc) &&
        this.rootDoc.defaultView
      ) {
        this.onFrameUnload({ window: this.rootDoc.defaultView });
      }
      // Update all DOM objects references to target the new document.
      this.rootWin = window;
      this.rootDoc = window.document;
      this.rootNode = this.document();
      this.emit("root-available", this.rootNode);
    } else {
      const frame = getFrameElement(window);
      const frameActor = this.getNode(frame);
      if (frameActor) {
        // If the parent frame is in the map of known node actors, create the
        // actor for the new document and emit a root-available event.
        const documentActor = this._getOrCreateNodeActor(window.document);
        this.emit("root-available", documentActor);
      }
    }
  },

  // Returns true if domNode is in window or a subframe.
  _childOfWindow: function(window, domNode) {
    while (domNode) {
      const win = nodeDocument(domNode).defaultView;
      if (win === window) {
        return true;
      }
      domNode = getFrameElement(win);
    }
    return false;
  },

  onFrameUnload: function({ window }) {
    // Any retained orphans that belong to this document
    // or its children need to be released, and a mutation sent
    // to notify of that.
    const releasedOrphans = [];

    for (const retained of this._retainedOrphans) {
      if (
        Cu.isDeadWrapper(retained.rawNode) ||
        this._childOfWindow(window, retained.rawNode)
      ) {
        this._retainedOrphans.delete(retained);
        releasedOrphans.push(retained.actorID);
        this.releaseNode(retained, { force: true });
      }
    }

    if (releasedOrphans.length > 0) {
      this.queueMutation({
        target: this.rootNode.actorID,
        type: "unretained",
        nodes: releasedOrphans,
      });
    }

    const doc = window.document;
    const documentActor = this.getNode(doc);
    if (!documentActor) {
      return;
    }

    // Removing a frame also removes any mutation breakpoints set on that
    // document so that clients can clear their set of active breakpoints.
    const mutationBps = this._mutationBreakpointsForDoc(doc);
    const nodes = mutationBps ? Array.from(mutationBps.nodes.keys()) : [];
    for (const node of nodes) {
      this._updateMutationBreakpointState("unload", node, null);
    }

    this.emit("root-destroyed", documentActor);

    // Cleanup root doc references if we just unloaded the top level root
    // document.
    if (this.rootDoc === doc) {
      this.rootDoc = null;
      this.rootNode = null;
    }

    // Release the actor for the unloaded document.
    this.releaseNode(documentActor, { force: true });
  },

  /**
   * Check if a node is attached to the DOM tree of the current page.
   * @param {Node} rawNode
   * @return {Boolean} false if the node is removed from the tree or within a
   * document fragment
   */
  _isInDOMTree: function(rawNode) {
    const walker = this.getDocumentWalker(rawNode);
    let current = walker.currentNode;

    // Reaching the top of tree
    while (walker.parentNode()) {
      current = walker.currentNode;
    }

    // The top of the tree is a fragment or is not rootDoc, hence rawNode isn't
    // attached
    if (
      current.nodeType === Node.DOCUMENT_FRAGMENT_NODE ||
      current !== this.rootDoc
    ) {
      return false;
    }

    // Otherwise the top of the tree is rootDoc, hence rawNode is in rootDoc
    return true;
  },

  /**
   * @see _isInDomTree
   */
  isInDOMTree: function(node) {
    if (isNodeDead(node)) {
      return false;
    }
    return this._isInDOMTree(node.rawNode);
  },

  /**
   * Given a windowID return the NodeActor for the corresponding frameElement,
   * unless it's the root window
   */
  getNodeActorFromWindowID: function(windowID) {
    let win;

    try {
      win = Services.wm.getOuterWindowWithId(windowID);
    } catch (e) {
      // ignore
    }

    if (!win) {
      return {
        error: "noWindow",
        message: "The related docshell is destroyed or not found",
      };
    } else if (!win.frameElement) {
      // the frame element of the root document is privileged & thus
      // inaccessible, so return the document body/element instead
      return this.attachElement(
        win.document.body || win.document.documentElement
      );
    }

    return this.attachElement(win.frameElement);
  },

  /**
   * Given a contentDomReference return the NodeActor for the corresponding frameElement.
   */
  getNodeActorFromContentDomReference: function(contentDomReference) {
    let rawNode = ContentDOMReference.resolve(contentDomReference);
    if (!rawNode || !this._isInDOMTree(rawNode)) {
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
  getStyleSheetOwnerNode: function(resourceId) {
    if (hasStyleSheetWatcherSupportForTarget(this.targetActor)) {
      const manager = this.targetActor.getStyleSheetManager();
      const ownerNode = manager.getOwnerNode(resourceId);
      return this.attachElement(ownerNode);
    }

    // Following code can be removed once we enable STYLESHEET resource on the watcher/server
    // side by default. For now it is being preffed off and we have to support the two
    // codepaths. Once enabled we will only support the stylesheet watcher codepath.
    const actorBasedNode = this.getNodeFromActor(resourceId, ["ownerNode"]);
    return actorBasedNode;
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
   * const inspectorFront = await toolbox.target.getFront("inspector");
   * // Retrieve the walker.
   * const walker = inspectorFront.walker;
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
  getNodeFromActor: function(actorID, path) {
    const actor = this.conn.getActor(actorID);
    if (!actor) {
      return null;
    }

    let obj = actor;
    for (const name of path) {
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
  getLayoutInspector: function() {
    if (!this.layoutActor) {
      this.layoutActor = new LayoutActor(this.conn, this.targetActor, this);
    }

    return this.layoutActor;
  },

  /**
   * Returns the parent grid DOMNode of the given node if it exists, otherwise, it
   * returns null.
   */
  getParentGridNode: function(node) {
    if (isNodeDead(node)) {
      return null;
    }

    const parentGridNode = findGridParentContainerForNode(node.rawNode);
    return parentGridNode ? this._getOrCreateNodeActor(parentGridNode) : null;
  },

  /**
   * Returns the offset parent DOMNode of the given node if it exists, otherwise, it
   * returns null.
   */
  getOffsetParent: function(node) {
    if (isNodeDead(node)) {
      return null;
    }

    const offsetParent = node.rawNode.offsetParent;

    if (!offsetParent) {
      return null;
    }

    return this._getOrCreateNodeActor(offsetParent);
  },

  getEmbedderElement(browsingContextID) {
    const browsingContext = BrowsingContext.get(browsingContextID);
    let rawNode = browsingContext.embedderElement;
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

  pick(doFocus) {
    this.nodePicker.pick(doFocus);
  },

  cancelPick() {
    this.nodePicker.cancelPick();
  },

  clearPicker() {
    this.nodePicker.resetHoveredNodeReference();
  },

  /**
   * Given a scrollable node, find its descendants which are causing overflow in it and
   * add their raw nodes to the map as keys with the scrollable element as the values.
   *
   * @param {NodeActor} scrollableNode A scrollable node.
   * @param {Map} map The map to which the overflow causing elements are added.
   */
  updateOverflowCausingElements: function(scrollableNode, map) {
    if (
      isNodeDead(scrollableNode) ||
      scrollableNode.rawNode.nodeType !== Node.ELEMENT_NODE
    ) {
      return;
    }

    const overflowCausingChildren = [
      ...InspectorUtils.getOverflowingChildrenOfElement(scrollableNode.rawNode),
    ];

    for (let overflowCausingChild of overflowCausingChildren) {
      // overflowCausingChild is a Node, but not necessarily an Element.
      // So, get the containing Element
      if (overflowCausingChild.nodeType !== Node.ELEMENT_NODE) {
        overflowCausingChild = overflowCausingChild.parentElement;
      }
      map.set(overflowCausingChild, scrollableNode);
    }
  },

  /**
   * Returns an array of the overflow causing elements' NodeActor for the given node.
   *
   * @param {NodeActor} node The scrollable node.
   * @return {Array<NodeActor>} An array of the overflow causing elements.
   */
  getOverflowCausingElements: function(node) {
    if (
      isNodeDead(node) ||
      node.rawNode.nodeType !== Node.ELEMENT_NODE ||
      !node.isScrollable
    ) {
      return [];
    }

    const overflowCausingElements = [
      ...InspectorUtils.getOverflowingChildrenOfElement(node.rawNode),
    ].map(overflowCausingChild => {
      if (overflowCausingChild.nodeType !== Node.ELEMENT_NODE) {
        overflowCausingChild = overflowCausingChild.parentElement;
      }

      return overflowCausingChild;
    });

    return this.attachElements(overflowCausingElements);
  },

  /**
   * Return the scrollable ancestor node which has overflow because of the given node.
   *
   * @param {NodeActor} overflowCausingNode
   */
  getScrollableAncestorNode: function(overflowCausingNode) {
    if (
      isNodeDead(overflowCausingNode) ||
      !this.overflowCausingElementsMap.has(overflowCausingNode.rawNode)
    ) {
      return null;
    }

    return this.overflowCausingElementsMap.get(overflowCausingNode.rawNode);
  },
});

exports.WalkerActor = WalkerActor;
