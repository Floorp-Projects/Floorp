/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

const Services = require("Services");
const protocol = require("devtools/shared/protocol");
const {walkerSpec} = require("devtools/shared/specs/inspector");
const {LongStringActor} = require("devtools/server/actors/string");
const InspectorUtils = require("InspectorUtils");

loader.lazyRequireGetter(this, "getFrameElement", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isAfterPseudoElement", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isAnonymous", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isBeforePseudoElement", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isDirectShadowHostChild", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isShadowHost", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isShadowRoot", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isTemplateElement", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "loadSheet", "devtools/shared/layout/utils", true);

loader.lazyRequireGetter(this, "throttle", "devtools/shared/throttle", true);

loader.lazyRequireGetter(this, "allAnonymousContentTreeWalkerFilter", "devtools/server/actors/inspector/utils", true);
loader.lazyRequireGetter(this, "isNodeDead", "devtools/server/actors/inspector/utils", true);
loader.lazyRequireGetter(this, "nodeDocument", "devtools/server/actors/inspector/utils", true);
loader.lazyRequireGetter(this, "standardTreeWalkerFilter", "devtools/server/actors/inspector/utils", true);

loader.lazyRequireGetter(this, "DocumentWalker", "devtools/server/actors/inspector/document-walker", true);
loader.lazyRequireGetter(this, "SKIP_TO_SIBLING", "devtools/server/actors/inspector/document-walker", true);
loader.lazyRequireGetter(this, "NodeActor", "devtools/server/actors/inspector/node", true);
loader.lazyRequireGetter(this, "NodeListActor", "devtools/server/actors/inspector/node", true);
loader.lazyRequireGetter(this, "LayoutActor", "devtools/server/actors/layout", true);
loader.lazyRequireGetter(this, "getLayoutChangesObserver", "devtools/server/actors/reflow", true);
loader.lazyRequireGetter(this, "releaseLayoutChangesObserver", "devtools/server/actors/reflow", true);
loader.lazyRequireGetter(this, "WalkerSearch", "devtools/server/actors/utils/walker-search", true);

loader.lazyServiceGetter(this, "eventListenerService",
  "@mozilla.org/eventlistenerservice;1", "nsIEventListenerService");

// Minimum delay between two "new-mutations" events.
const MUTATIONS_THROTTLING_DELAY = 100;
// List of mutation types that should -not- be throttled.
const IMMEDIATE_MUTATIONS = [
  "documentUnload",
  "frameLoad",
  "newRoot",
  "pseudoClassLock",
];

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
  ["::selection", 0]
];

const HELPER_SHEET = "data:text/css;charset=utf-8," + encodeURIComponent(`
  .__fx-devtools-hide-shortcut__ {
    visibility: hidden !important;
  }

  :-moz-devtools-highlighted {
    outline: 2px dashed #F06!important;
    outline-offset: -2px !important;
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
   * @param DebuggerServerConnection conn
   *    The server connection.
   */
  initialize: function(conn, targetActor, options) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
    this.rootWin = targetActor.window;
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
    this.onSlotchange = this.onSlotchange.bind(this);
    this.onShadowrootattached = this.onShadowrootattached.bind(this);
    this.onFrameLoad = this.onFrameLoad.bind(this);
    this.onFrameUnload = this.onFrameUnload.bind(this);
    this._throttledEmitNewMutations = throttle(this._emitNewMutations.bind(this),
      MUTATIONS_THROTTLING_DELAY);

    targetActor.on("will-navigate", this.onFrameUnload);
    targetActor.on("window-ready", this.onFrameLoad);

    // Keep a reference to the chromeEventHandler for the current targetActor, to make
    // sure we will be able to remove the listener during the WalkerActor destroy().
    this.chromeEventHandler = targetActor.chromeEventHandler;
    // shadowrootattached is a chrome-only event.
    this.chromeEventHandler.addEventListener("shadowrootattached",
      this.onShadowrootattached);

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

  /**
   * Callback for eventListenerService.addListenerChangeListener
   * @param nsISimpleEnumerator changesEnum
   *    enumerator of nsIEventListenerChange
   */
  _onEventListenerChange: function(changesEnum) {
    const changes = changesEnum.enumerate();
    while (changes.hasMoreElements()) {
      const current = changes.getNext().QueryInterface(Ci.nsIEventListenerChange);
      const target = current.target;

      if (this._refMap.has(target)) {
        const actor = this.getNode(target);
        const mutation = {
          type: "events",
          target: actor.actorID,
          hasEventListeners: actor._hasEventListeners
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
      traits: {}
    };
  },

  toString: function() {
    return "[WalkerActor " + this.actorID + "]";
  },

  getDocumentWalker: function(node, whatToShow, skipTo) {
    // Allow native anon content (like <video> controls) if preffed on
    const filter = this.showAllAnonymousContent
                    ? allAnonymousContentTreeWalkerFilter
                    : standardTreeWalkerFilter;

    return new DocumentWalker(node, this.rootWin,
      {whatToShow, filter, skipTo, showAnonymousContent: true});
  },

  getNonAnonymousWalker: function(node, whatToShow, skipTo) {
    const nodeFilter = standardTreeWalkerFilter;

    return new DocumentWalker(node, this.rootWin,
      {whatToShow, nodeFilter, skipTo, showAnonymousContent: false});
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

      this._hoveredNode = null;
      this.rootWin = null;
      this.rootDoc = null;
      this.rootNode = null;
      this.layoutHelpers = null;
      this._orphaned = null;
      this._retainedOrphans = null;
      this._refMap = null;

      this.targetActor.off("will-navigate", this.onFrameUnload);
      this.targetActor.off("window-ready", this.onFrameLoad);
      this.chromeEventHandler.removeEventListener("shadowrootattached",
        this.onShadowrootattached);

      this.onFrameLoad = null;
      this.onFrameUnload = null;

      this.walkerSearch.destroy();

      this.layoutChangeObserver.off("reflows", this._onReflows);
      this.layoutChangeObserver.off("resize", this._onResize);
      this.layoutChangeObserver = null;
      releaseLayoutChangesObserver(this.targetActor);

      eventListenerService.removeListenerChangeListener(
        this._onEventListenerChange);

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
  hasNode: function(rawNode) {
    return this._refMap.has(rawNode);
  },

  /**
   * If the walker has come across this DOM node before, then get the
   * corresponding node actor.
   * @param {DOMNode} rawNode
   * @return {NodeActor}
   */
  getNode: function(rawNode) {
    return this._refMap.get(rawNode);
  },

  _ref: function(node) {
    let actor = this.getNode(node);
    if (actor) {
      return actor;
    }

    actor = new NodeActor(this, node);

    // Add the node actor as a child of this walker actor, assigning
    // it an actorID.
    this.manage(actor);
    this._refMap.set(node, actor);

    if (node.nodeType === Node.DOCUMENT_NODE) {
      actor.watchDocument(node, this.onMutations);
    }

    if (isShadowRoot(actor.rawNode)) {
      actor.watchDocument(node.ownerDocument, this.onMutations);
      actor.watchSlotchange(this.onSlotchange);
    }

    return actor;
  },

  _onReflows: function(reflows) {
    // Going through the nodes the walker knows about, see which ones have
    // had their display changed and send a display-change event if any
    const changes = [];
    for (const [node, actor] of this._refMap) {
      if (Cu.isDeadWrapper(node)) {
        continue;
      }

      const displayType = actor.displayType;
      const isDisplayed = actor.isDisplayed;

      if (displayType !== actor.currentDisplayType ||
          isDisplayed !== actor.wasDisplayed) {
        changes.push(actor);

        // Updating the original value
        actor.currentDisplayType = displayType;
        actor.wasDisplayed = isDisplayed;
      }
    }

    if (changes.length) {
      this.emit("display-change", changes);
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
      newParents: newParents
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
  document: function(node) {
    const doc = isNodeDead(node) ? this.rootDoc : nodeDocument(node.rawNode);
    return this._ref(doc);
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
    return this._ref(elt);
  },

  parentNode: function(node) {
    const parent = this.rawParentNode(node);
    if (parent) {
      return this._ref(parent);
    }

    return null;
  },

  rawParentNode: function(node) {
    let parent;
    try {
      // If the node is the child of a shadow host, we can not use an anonymous walker to
      // get the shadow host parent.
      const walker = isDirectShadowHostChild(node.rawNode)
        ? this.getNonAnonymousWalker(node.rawNode)
        : this.getDocumentWalker(node.rawNode);
      parent = walker.parentNode();
    } catch (e) {
      // When getting the parent node for a child of a non-slotted shadow host child,
      // walker.parentNode() will throw if the walker is anonymous, because non-slotted
      // shadow host children are not accessible anywhere in the anonymous tree.
      const walker = this.getNonAnonymousWalker(node.rawNode);
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
  inlineTextChild: function(node) {
    // Quick checks to prevent creating a new walker if possible.
    if (isBeforePseudoElement(node.rawNode) ||
        isAfterPseudoElement(node.rawNode) ||
        node.rawNode.nodeType != Node.ELEMENT_NODE ||
        node.rawNode.children.length > 0) {
      return undefined;
    }

    const walker = isDirectShadowHostChild(node.rawNode)
      ? this.getNonAnonymousWalker(node.rawNode)
      : this.getDocumentWalker(node.rawNode);
    const firstChild = walker.firstChild();

    // Bail out if:
    // - more than one child
    // - unique child is not a text node
    // - unique child is a text node, but is too long to be inlined
    if (!firstChild ||
        walker.nextSibling() ||
        firstChild.nodeType !== Node.TEXT_NODE ||
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
      parentActor = this._ref(parent);
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
      nodes: nodes.map(n => this._ref(n))
    };
  },

  /**
   * Return chidlren of the given node. Contrary to children children(), this method only
   * returns DOMNodes. Therefore it will not create NodeActor wrappers and will not
   * update the refMap for the discovered nodes either. This makes this method safe to
   * call when you are not sure if the discovered nodes will be communicated to the
   * client.
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
        this.getDocumentWalker(node.rawNode, options.whatToShow, SKIP_TO_SIBLING);
      } catch (e) {
        isUnslottedHostChild = true;
      }
    }

    // We're going to create a few document walkers with the same filter,
    // make it easier.
    const getFilteredWalker = documentWalkerNode => {
      const { whatToShow } = options;

      // Use SKIP_TO_SIBLING to force the walker to use a sibling of the provided node
      // in case this one is incompatible with the walker's filter function.
      const skipTo = SKIP_TO_SIBLING;

      const useNonAnonymousWalker = shadowRoot || shadowHost || isUnslottedHostChild;
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
        return this.getNonAnonymousWalker(documentWalkerNode, whatToShow, skipTo);
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

      // A shadow root is not included in the children returned by the walker, so we can
      // not use it as start node. However it will be displayed as the first node, so
      // we use firstChild as a fallback.
      if (isShadowRoot(start)) {
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
      // Use anonymous walkers to fetch ::before / ::after pseudo elements
      const firstChildWalker = this.getDocumentWalker(node.rawNode);
      const first = firstChildWalker.firstChild();
      const hasBefore = first && first.nodeName === "_moz_generated_content_before";

      const lastChildWalker = this.getDocumentWalker(node.rawNode);
      const last = lastChildWalker.lastChild();
      const hasAfter = last && last.nodeName === "_moz_generated_content_after";

      nodes = [
        // #shadow-root
        node.rawNode.openOrClosedShadowRoot,
        // ::before
        ...(hasBefore ? [first] : []),
        // shadow host direct children
        ...nodes,
        // ::after
        ...(hasAfter ? [last] : []),
      ];
    }

    return { hasFirst, hasLast, nodes };
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
  previousSibling: function(node, options = {}) {
    if (isNodeDead(node)) {
      return null;
    }

    const walker = this.getDocumentWalker(node.rawNode, options.whatToShow);
    const sibling = walker.previousSibling();
    return sibling ? this._ref(sibling) : null;
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

    for (const {document} of this.targetActor.windows) {
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
  multiFrameQuerySelectorAll: function(selector) {
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
  search: function(query) {
    const results = this.walkerSearch.search(query);
    const nodeList = new NodeListActor(this, results.map(r => r.node));

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
  getSuggestionsForQuery: function(query, completing, selectorState) {
    const sugs = {
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
        for (const node of nodes) {
          for (const className of node.classList) {
            sugs.classes.set(className, (sugs.classes.get(className)|0) + 1);
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
          sugs.ids.set(node.id, (sugs.ids.get(node.id)|0) + 1);
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
          sugs.tags.set(tag, (sugs.tags.get(tag)|0) + 1);
        }
        for (const [tag, count] of sugs.tags) {
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
        for (const node of nodes) {
          sugs.ids.set(node.id, (sugs.ids.get(node.id)|0) + 1);
          const tag = node.localName;
          sugs.tags.set(tag, (sugs.tags.get(tag)|0) + 1);
          for (const className of node.classList) {
            sugs.classes.set(className, (sugs.classes.get(className)|0) + 1);
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
      let sortA = (10000 - a[1]) + a[0];
      let sortB = (10000 - b[1]) + b[0];

      // Prefixing ids, classes and tags, to group results
      const firstA = a[0].substring(0, 1);
      const firstB = b[0].substring(0, 1);

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

    const enabled = options.enabled === undefined ||
                  options.enabled;
    this._addPseudoClassLock(node, pseudo, enabled);

    if (!options.parents) {
      return;
    }

    const walker = this.getDocumentWalker(node.rawNode);
    let cur;
    while ((cur = walker.parentNode())) {
      const curNode = this._ref(cur);
      this._addPseudoClassLock(curNode, pseudo, enabled);
    }
  },

  _queuePseudoClassMutation: function(node) {
    this.queueMutation({
      target: node.actorID,
      type: "pseudoClassLock",
      pseudoClassLocks: node.writePseudoClassLocks()
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
   *    A pseudoclass: ':hover', ':active', ':focus'
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
      if (node.rawNode.contains(locked.rawNode) &&
          InspectorUtils.hasPseudoClassLock(locked.rawNode, pseudo)) {
        this._removePseudoClassLock(locked, pseudo);
      }
    }

    if (!options.parents) {
      return;
    }

    const walker = this.getDocumentWalker(node.rawNode);
    let cur;
    while ((cur = walker.parentNode())) {
      const curNode = this._ref(cur);
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
    if (rawNode.nodeType !== rawNode.ownerDocument.ELEMENT_NODE) {
      throw new Error("Can only change innerHTML to element nodes");
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

    const parsedDOM = new DOMParser().parseFromString(value, "text/html");
    const rawNode = node.rawNode;
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
      // Unable to set outerHTML on the document element.  Fall back by
      // setting attributes manually, then replace the body and head elements.
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
          newValue: attributeModifications[key]
        });
      }
      node.modifyAttributes(finalAttributeModifications);
      rawNode.replaceChild(parsedDOM.head, rawNode.querySelector("head"));
      rawNode.replaceChild(parsedDOM.body, rawNode.querySelector("body"));
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
      return {node: [], newParents: []};
    }

    const rawNode = node.rawNode;
    const isInsertAsSibling = position === "beforeBegin" ||
      position === "afterEnd";

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
  duplicateNode: function({rawNode}) {
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
    return ((node.rawNode.ownerDocument &&
      node.rawNode.ownerDocument.documentElement === this.rawNode) ||
      node.rawNode.nodeType === Node.DOCUMENT_NODE);
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
    if (isNodeDead(node) ||
        isNodeDead(parent) ||
        (sibling && isNodeDead(sibling))) {
      return;
    }

    const rawNode = node.rawNode;
    const rawParent = parent.rawNode;
    const rawSibling = sibling ? sibling.rawNode : null;

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
      return Promise.reject(new Error("Could not change node's tagName to " + tagName));
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
        mutation.newValue = targetNode.hasAttribute(mutation.attributeName) ?
                            targetNode.getAttribute(mutation.attributeName)
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
    const {oldValue, target} = mutation;
    const newValue = target.nodeValue;
    const limit = gValueSummaryLength;

    if ((oldValue.length <= limit && newValue.length <= limit) ||
        (oldValue.length > limit && newValue.length > limit)) {
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
      inlineTextChild:
        inlineTextChild ? inlineTextChild.form() : undefined
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
      target: targetActor.actorID
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
    const { readyState } = window.document;
    if (readyState != "interactive" && readyState != "complete") {
      window.addEventListener("DOMContentLoaded",
        this.onFrameLoad.bind(this, { window, isTopLevel }),
        { once: true });
      return;
    }
    if (isTopLevel) {
      // If we initialize the inspector while the document is loading,
      // we may already have a root document set in the constructor.
      if (this.rootDoc && !Cu.isDeadWrapper(this.rootDoc) &&
          this.rootDoc.defaultView) {
        this.onFrameUnload({ window: this.rootDoc.defaultView });
      }
      // Update all DOM objects references to target the new document.
      this.rootWin = window;
      this.rootDoc = window.document;
      this.rootNode = this.document();
      this.queueMutation({
        type: "newRoot",
        target: this.rootNode.form()
      });
      return;
    }
    const frame = getFrameElement(window);
    const frameActor = this.getNode(frame);
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
  _childOfWindow: function(window, domNode) {
    let win = nodeDocument(domNode).defaultView;
    while (win) {
      if (win === window) {
        return true;
      }
      win = getFrameElement(win);
    }
    return false;
  },

  onFrameUnload: function({ window }) {
    // Any retained orphans that belong to this document
    // or its children need to be released, and a mutation sent
    // to notify of that.
    const releasedOrphans = [];

    for (const retained of this._retainedOrphans) {
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

    const doc = window.document;
    const documentActor = this.getNode(doc);
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

    const walker = this.getDocumentWalker(doc);
    const parentNode = walker.parentNode();
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
    if (current.nodeType === Node.DOCUMENT_FRAGMENT_NODE ||
        current !== this.rootDoc) {
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
   * Given an ObjectActor (identified by its ID), commonly used in the debugger,
   * webconsole and variablesView, return the corresponding inspector's
   * NodeActor
   */
  getNodeActorFromObjectActor: function(objectActorID) {
    const actor = this.conn.getActor(objectActorID);
    if (!actor) {
      return null;
    }

    const debuggerObject = this.conn.getActor(objectActorID).obj;
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
      return { error: "noWindow",
               message: "The related docshell is destroyed or not found" };
    } else if (!win.frameElement) {
      // the frame element of the root document is privileged & thus
      // inaccessible, so return the document body/element instead
      return this.attachElement(win.document.body || win.document.documentElement);
    }

    return this.attachElement(win.frameElement);
  },

  /**
   * Given a StyleSheetActor (identified by its ID), commonly used in the
   * style-editor, get its ownerNode and return the corresponding walker's
   * NodeActor.
   * Note that getNodeFromActor was added later and can now be used instead.
   */
  getStyleSheetOwnerNode: function(styleSheetActorID) {
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

    return this._ref(offsetParent);
  },

  /**
   * Returns true if accessibility service is running and the node has a
   * corresponding valid accessible object.
   */
  hasAccessibilityProperties: async function(node) {
    if (isNodeDead(node) || !Services.appinfo.accessibilityEnabled) {
      return false;
    }

    const accService = Cc["@mozilla.org/accessibilityService;1"].getService(
      Ci.nsIAccessibilityService);
    let acc = accService.getAccessibleFor(node.rawNode);
    // If node does not have an accessible object, but has an inline text child,
    // try to retrieve an accessible object for the child instead.
    if (!acc || acc.indexInParent < 0) {
      const inlineTextChild = this.inlineTextChild(node);
      if (inlineTextChild) {
        acc = accService.getAccessibleFor(inlineTextChild.rawNode);
      }
    }

    return acc && acc.indexInParent > -1;
  },
});

exports.WalkerActor = WalkerActor;
