/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Services = require("Services");
const { Ci } = require("chrome");
require("devtools/client/fronts/styles");
require("devtools/client/fronts/highlighters");
const { ShortLongString } = require("devtools/server/actors/string");
const {
  Front,
  FrontClassWithSpec,
  custom,
  preEvent,
  types
} = require("devtools/shared/protocol.js");
const { makeInfallible } = require("devtools/shared/DevToolsUtils");
const {
  inspectorSpec,
  nodeSpec,
  nodeListSpec,
  walkerSpec
} = require("devtools/shared/specs/inspector");
const promise = require("promise");
const { Task } = require("resource://gre/modules/Task.jsm");
const { Class } = require("sdk/core/heritage");
const events = require("sdk/event/core");
const object = require("sdk/util/object");

const HIDDEN_CLASS = "__fx-devtools-hide-shortcut__";

/**
 * Convenience API for building a list of attribute modifications
 * for the `modifyAttributes` request.
 */
const AttributeModificationList = Class({
  initialize: function (node) {
    this.node = node;
    this.modifications = [];
  },

  apply: function () {
    let ret = this.node.modifyAttributes(this.modifications);
    return ret;
  },

  destroy: function () {
    this.node = null;
    this.modification = null;
  },

  setAttributeNS: function (ns, name, value) {
    this.modifications.push({
      attributeNamespace: ns,
      attributeName: name,
      newValue: value
    });
  },

  setAttribute: function (name, value) {
    this.setAttributeNS(undefined, name, value);
  },

  removeAttributeNS: function (ns, name) {
    this.setAttributeNS(ns, name, undefined);
  },

  removeAttribute: function (name) {
    this.setAttributeNS(undefined, name, undefined);
  }
});

// A resolve that hits the main loop first.
function delayedResolve(value) {
  let deferred = promise.defer();
  Services.tm.mainThread.dispatch(makeInfallible(() => {
    deferred.resolve(value);
  }), 0);
  return deferred.promise;
}

/**
 * Client side of the node actor.
 *
 * Node fronts are strored in a tree that mirrors the DOM tree on the
 * server, but with a few key differences:
 *  - Not all children will be necessary loaded for each node.
 *  - The order of children isn't guaranteed to be the same as the DOM.
 * Children are stored in a doubly-linked list, to make addition/removal
 * and traversal quick.
 *
 * Due to the order/incompleteness of the child list, it is safe to use
 * the parent node from clients, but the `children` request should be used
 * to traverse children.
 */
const NodeFront = FrontClassWithSpec(nodeSpec, {
  initialize: function (conn, form, detail, ctx) {
    // The parent node
    this._parent = null;
    // The first child of this node.
    this._child = null;
    // The next sibling of this node.
    this._next = null;
    // The previous sibling of this node.
    this._prev = null;
    Front.prototype.initialize.call(this, conn, form, detail, ctx);
  },

  /**
   * Destroy a node front.  The node must have been removed from the
   * ownership tree before this is called, unless the whole walker front
   * is being destroyed.
   */
  destroy: function () {
    Front.prototype.destroy.call(this);
  },

  // Update the object given a form representation off the wire.
  form: function (form, detail, ctx) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    // Shallow copy of the form.  We could just store a reference, but
    // eventually we'll want to update some of the data.
    this._form = object.merge(form);
    this._form.attrs = this._form.attrs ? this._form.attrs.slice() : [];

    if (form.parent) {
      // Get the owner actor for this actor (the walker), and find the
      // parent node of this actor from it, creating a standin node if
      // necessary.
      let parentNodeFront = ctx.marshallPool().ensureParentFront(form.parent);
      this.reparent(parentNodeFront);
    }

    if (form.singleTextChild) {
      this.singleTextChild =
        types.getType("domnode").read(form.singleTextChild, ctx);
    } else {
      this.singleTextChild = undefined;
    }
  },

  /**
   * Returns the parent NodeFront for this NodeFront.
   */
  parentNode: function () {
    return this._parent;
  },

  /**
   * Process a mutation entry as returned from the walker's `getMutations`
   * request.  Only tries to handle changes of the node's contents
   * themselves (character data and attribute changes), the walker itself
   * will keep the ownership tree up to date.
   */
  updateMutation: function (change) {
    if (change.type === "attributes") {
      // We'll need to lazily reparse the attributes after this change.
      this._attrMap = undefined;

      // Update any already-existing attributes.
      let found = false;
      for (let i = 0; i < this.attributes.length; i++) {
        let attr = this.attributes[i];
        if (attr.name == change.attributeName &&
            attr.namespace == change.attributeNamespace) {
          if (change.newValue !== null) {
            attr.value = change.newValue;
          } else {
            this.attributes.splice(i, 1);
          }
          found = true;
          break;
        }
      }
      // This is a new attribute. The null check is because of Bug 1192270,
      // in the case of a newly added then removed attribute
      if (!found && change.newValue !== null) {
        this.attributes.push({
          name: change.attributeName,
          namespace: change.attributeNamespace,
          value: change.newValue
        });
      }
    } else if (change.type === "characterData") {
      this._form.shortValue = change.newValue;
      this._form.incompleteValue = change.incompleteValue;
    } else if (change.type === "pseudoClassLock") {
      this._form.pseudoClassLocks = change.pseudoClassLocks;
    } else if (change.type === "events") {
      this._form.hasEventListeners = change.hasEventListeners;
    }
  },

  // Some accessors to make NodeFront feel more like an nsIDOMNode

  get id() {
    return this.getAttribute("id");
  },

  get nodeType() {
    return this._form.nodeType;
  },
  get namespaceURI() {
    return this._form.namespaceURI;
  },
  get nodeName() {
    return this._form.nodeName;
  },
  get doctypeString() {
    return "<!DOCTYPE " + this._form.name +
     (this._form.publicId ? " PUBLIC \"" + this._form.publicId + "\"" : "") +
     (this._form.systemId ? " \"" + this._form.systemId + "\"" : "") +
     ">";
  },

  get baseURI() {
    return this._form.baseURI;
  },

  get className() {
    return this.getAttribute("class") || "";
  },

  get hasChildren() {
    return this._form.numChildren > 0;
  },
  get numChildren() {
    return this._form.numChildren;
  },
  get hasEventListeners() {
    return this._form.hasEventListeners;
  },

  get isBeforePseudoElement() {
    return this._form.isBeforePseudoElement;
  },
  get isAfterPseudoElement() {
    return this._form.isAfterPseudoElement;
  },
  get isPseudoElement() {
    return this.isBeforePseudoElement || this.isAfterPseudoElement;
  },
  get isAnonymous() {
    return this._form.isAnonymous;
  },
  get isInHTMLDocument() {
    return this._form.isInHTMLDocument;
  },
  get tagName() {
    return this.nodeType === Ci.nsIDOMNode.ELEMENT_NODE ? this.nodeName : null;
  },
  get shortValue() {
    return this._form.shortValue;
  },
  get incompleteValue() {
    return !!this._form.incompleteValue;
  },

  get isDocumentElement() {
    return !!this._form.isDocumentElement;
  },

  // doctype properties
  get name() {
    return this._form.name;
  },
  get publicId() {
    return this._form.publicId;
  },
  get systemId() {
    return this._form.systemId;
  },

  getAttribute: function (name) {
    let attr = this._getAttribute(name);
    return attr ? attr.value : null;
  },
  hasAttribute: function (name) {
    this._cacheAttributes();
    return (name in this._attrMap);
  },

  get hidden() {
    let cls = this.getAttribute("class");
    return cls && cls.indexOf(HIDDEN_CLASS) > -1;
  },

  get attributes() {
    return this._form.attrs;
  },

  get pseudoClassLocks() {
    return this._form.pseudoClassLocks || [];
  },
  hasPseudoClassLock: function (pseudo) {
    return this.pseudoClassLocks.some(locked => locked === pseudo);
  },

  get isDisplayed() {
    // The NodeActor's form contains the isDisplayed information as a boolean
    // starting from FF32. Before that, the property is missing
    return "isDisplayed" in this._form ? this._form.isDisplayed : true;
  },

  get isTreeDisplayed() {
    let parent = this;
    while (parent) {
      if (!parent.isDisplayed) {
        return false;
      }
      parent = parent.parentNode();
    }
    return true;
  },

  getNodeValue: custom(function () {
    if (!this.incompleteValue) {
      return delayedResolve(new ShortLongString(this.shortValue));
    }

    return this._getNodeValue();
  }, {
    impl: "_getNodeValue"
  }),

  // Accessors for custom form properties.

  getFormProperty: function (name) {
    return this._form.props ? this._form.props[name] : null;
  },

  hasFormProperty: function (name) {
    return this._form.props ? (name in this._form.props) : null;
  },

  get formProperties() {
    return this._form.props;
  },

  /**
   * Return a new AttributeModificationList for this node.
   */
  startModifyingAttributes: function () {
    return AttributeModificationList(this);
  },

  _cacheAttributes: function () {
    if (typeof this._attrMap != "undefined") {
      return;
    }
    this._attrMap = {};
    for (let attr of this.attributes) {
      this._attrMap[attr.name] = attr;
    }
  },

  _getAttribute: function (name) {
    this._cacheAttributes();
    return this._attrMap[name] || undefined;
  },

  /**
   * Set this node's parent.  Note that the children saved in
   * this tree are unordered and incomplete, so shouldn't be used
   * instead of a `children` request.
   */
  reparent: function (parent) {
    if (this._parent === parent) {
      return;
    }

    if (this._parent && this._parent._child === this) {
      this._parent._child = this._next;
    }
    if (this._prev) {
      this._prev._next = this._next;
    }
    if (this._next) {
      this._next._prev = this._prev;
    }
    this._next = null;
    this._prev = null;
    this._parent = parent;
    if (!parent) {
      // Subtree is disconnected, we're done
      return;
    }
    this._next = parent._child;
    if (this._next) {
      this._next._prev = this;
    }
    parent._child = this;
  },

  /**
   * Return all the known children of this node.
   */
  treeChildren: function () {
    let ret = [];
    for (let child = this._child; child != null; child = child._next) {
      ret.push(child);
    }
    return ret;
  },

  /**
   * Do we use a local target?
   * Useful to know if a rawNode is available or not.
   *
   * This will, one day, be removed. External code should
   * not need to know if the target is remote or not.
   */
  isLocalToBeDeprecated: function () {
    return !!this.conn._transport._serverConnection;
  },

  /**
   * Get an nsIDOMNode for the given node front.  This only works locally,
   * and is only intended as a stopgap during the transition to the remote
   * protocol.  If you depend on this you're likely to break soon.
   */
  rawNode: function (rawNode) {
    if (!this.conn._transport._serverConnection) {
      console.warn("Tried to use rawNode on a remote connection.");
      return null;
    }
    let actor = this.conn._transport._serverConnection.getActor(this.actorID);
    if (!actor) {
      // Can happen if we try to get the raw node for an already-expired
      // actor.
      return null;
    }
    return actor.rawNode;
  }
});

exports.NodeFront = NodeFront;

/**
 * Client side of a node list as returned by querySelectorAll()
 */
const NodeListFront = FrontClassWithSpec(nodeListSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client, form);
  },

  destroy: function () {
    Front.prototype.destroy.call(this);
  },

  marshallPool: function () {
    return this.parent();
  },

  // Update the object given a form representation off the wire.
  form: function (json) {
    this.length = json.length;
  },

  item: custom(function (index) {
    return this._item(index).then(response => {
      return response.node;
    });
  }, {
    impl: "_item"
  }),

  items: custom(function (start, end) {
    return this._items(start, end).then(response => {
      return response.nodes;
    });
  }, {
    impl: "_items"
  })
});

exports.NodeListFront = NodeListFront;

/**
 * Client side of the DOM walker.
 */
const WalkerFront = FrontClassWithSpec(walkerSpec, {
  // Set to true if cleanup should be requested after every mutation list.
  autoCleanup: true,

  /**
   * This is kept for backward-compatibility reasons with older remote target.
   * Targets previous to bug 916443
   */
  pick: custom(function () {
    return this._pick().then(response => {
      return response.node;
    });
  }, {impl: "_pick"}),

  initialize: function (client, form) {
    this._createRootNodePromise();
    Front.prototype.initialize.call(this, client, form);
    this._orphaned = new Set();
    this._retainedOrphans = new Set();
  },

  destroy: function () {
    Front.prototype.destroy.call(this);
  },

  // Update the object given a form representation off the wire.
  form: function (json) {
    this.actorID = json.actor;
    this.rootNode = types.getType("domnode").read(json.root, this);
    this._rootNodeDeferred.resolve(this.rootNode);
    // FF42+ the actor starts exposing traits
    this.traits = json.traits || {};
  },

  /**
   * Clients can use walker.rootNode to get the current root node of the
   * walker, but during a reload the root node might be null.  This
   * method returns a promise that will resolve to the root node when it is
   * set.
   */
  getRootNode: function () {
    return this._rootNodeDeferred.promise;
  },

  /**
   * Create the root node promise, triggering the "new-root" notification
   * on resolution.
   */
  _createRootNodePromise: function () {
    this._rootNodeDeferred = promise.defer();
    this._rootNodeDeferred.promise.then(() => {
      events.emit(this, "new-root");
    });
  },

  /**
   * When reading an actor form off the wire, we want to hook it up to its
   * parent front.  The protocol guarantees that the parent will be seen
   * by the client in either a previous or the current request.
   * So if we've already seen this parent return it, otherwise create
   * a bare-bones stand-in node.  The stand-in node will be updated
   * with a real form by the end of the deserialization.
   */
  ensureParentFront: function (id) {
    let front = this.get(id);
    if (front) {
      return front;
    }

    return types.getType("domnode").read({ actor: id }, this, "standin");
  },

  /**
   * See the documentation for WalkerActor.prototype.retainNode for
   * information on retained nodes.
   *
   * From the client's perspective, `retainNode` can fail if the node in
   * question is removed from the ownership tree before the `retainNode`
   * request reaches the server.  This can only happen if the client has
   * asked the server to release nodes but hasn't gotten a response
   * yet: Either a `releaseNode` request or a `getMutations` with `cleanup`
   * set is outstanding.
   *
   * If either of those requests is outstanding AND releases the retained
   * node, this request will fail with noSuchActor, but the ownership tree
   * will stay in a consistent state.
   *
   * Because the protocol guarantees that requests will be processed and
   * responses received in the order they were sent, we get the right
   * semantics by setting our local retained flag on the node only AFTER
   * a SUCCESSFUL retainNode call.
   */
  retainNode: custom(function (node) {
    return this._retainNode(node).then(() => {
      node.retained = true;
    });
  }, {
    impl: "_retainNode",
  }),

  unretainNode: custom(function (node) {
    return this._unretainNode(node).then(() => {
      node.retained = false;
      if (this._retainedOrphans.has(node)) {
        this._retainedOrphans.delete(node);
        this._releaseFront(node);
      }
    });
  }, {
    impl: "_unretainNode"
  }),

  releaseNode: custom(function (node, options = {}) {
    // NodeFront.destroy will destroy children in the ownership tree too,
    // mimicking what the server will do here.
    let actorID = node.actorID;
    this._releaseFront(node, !!options.force);
    return this._releaseNode({ actorID: actorID });
  }, {
    impl: "_releaseNode"
  }),

  findInspectingNode: custom(function () {
    return this._findInspectingNode().then(response => {
      return response.node;
    });
  }, {
    impl: "_findInspectingNode"
  }),

  querySelector: custom(function (queryNode, selector) {
    return this._querySelector(queryNode, selector).then(response => {
      return response.node;
    });
  }, {
    impl: "_querySelector"
  }),

  getNodeActorFromObjectActor: custom(function (objectActorID) {
    return this._getNodeActorFromObjectActor(objectActorID).then(response => {
      return response ? response.node : null;
    });
  }, {
    impl: "_getNodeActorFromObjectActor"
  }),

  getStyleSheetOwnerNode: custom(function (styleSheetActorID) {
    return this._getStyleSheetOwnerNode(styleSheetActorID).then(response => {
      return response ? response.node : null;
    });
  }, {
    impl: "_getStyleSheetOwnerNode"
  }),

  getNodeFromActor: custom(function (actorID, path) {
    return this._getNodeFromActor(actorID, path).then(response => {
      return response ? response.node : null;
    });
  }, {
    impl: "_getNodeFromActor"
  }),

  /*
   * Incrementally search the document for a given string.
   * For modern servers, results will be searched with using the WalkerActor
   * `search` function (includes tag names, attributes, and text contents).
   * Only 1 result is sent back, and calling the method again with the same
   * query will send the next result. When there are no more results to be sent
   * back, null is sent.
   * @param {String} query
   * @param {Object} options
   *    - "reverse": search backwards
   *    - "selectorOnly": treat input as a selector string (don't search text
   *                      tags, attributes, etc)
   */
  search: custom(Task.async(function* (query, options = { }) {
    let nodeList;
    let searchType;
    let searchData = this.searchData = this.searchData || { };
    let selectorOnly = !!options.selectorOnly;

    // Backwards compat.  Use selector only search if the new
    // search functionality isn't implemented, or if the caller (tests)
    // want it.
    if (selectorOnly || !this.traits.textSearch) {
      searchType = "selector";
      if (this.traits.multiFrameQuerySelectorAll) {
        nodeList = yield this.multiFrameQuerySelectorAll(query);
      } else {
        nodeList = yield this.querySelectorAll(this.rootNode, query);
      }
    } else {
      searchType = "search";
      let result = yield this._search(query, options);
      nodeList = result.list;
    }

    // If this is a new search, start at the beginning.
    if (searchData.query !== query ||
        searchData.selectorOnly !== selectorOnly) {
      searchData.selectorOnly = selectorOnly;
      searchData.query = query;
      searchData.index = -1;
    }

    if (!nodeList.length) {
      return null;
    }

    // Move search result cursor and cycle if necessary.
    searchData.index = options.reverse ? searchData.index - 1 :
                                         searchData.index + 1;
    if (searchData.index >= nodeList.length) {
      searchData.index = 0;
    }
    if (searchData.index < 0) {
      searchData.index = nodeList.length - 1;
    }

    // Send back the single node, along with any relevant search data
    let node = yield nodeList.item(searchData.index);
    return {
      type: searchType,
      node: node,
      resultsLength: nodeList.length,
      resultsIndex: searchData.index,
    };
  }), {
    impl: "_search"
  }),

  _releaseFront: function (node, force) {
    if (node.retained && !force) {
      node.reparent(null);
      this._retainedOrphans.add(node);
      return;
    }

    if (node.retained) {
      // Forcing a removal.
      this._retainedOrphans.delete(node);
    }

    // Release any children
    for (let child of node.treeChildren()) {
      this._releaseFront(child, force);
    }

    // All children will have been removed from the node by this point.
    node.reparent(null);
    node.destroy();
  },

  /**
   * Get any unprocessed mutation records and process them.
   */
  getMutations: custom(function (options = {}) {
    return this._getMutations(options).then(mutations => {
      let emitMutations = [];
      for (let change of mutations) {
        // The target is only an actorID, get the associated front.
        let targetID;
        let targetFront;

        if (change.type === "newRoot") {
          this.rootNode = types.getType("domnode").read(change.target, this);
          this._rootNodeDeferred.resolve(this.rootNode);
          targetID = this.rootNode.actorID;
          targetFront = this.rootNode;
        } else {
          targetID = change.target;
          targetFront = this.get(targetID);
        }

        if (!targetFront) {
          console.trace("Got a mutation for an unexpected actor: " + targetID +
            ", please file a bug on bugzilla.mozilla.org!");
          continue;
        }

        let emittedMutation = object.merge(change, { target: targetFront });

        if (change.type === "childList" ||
            change.type === "nativeAnonymousChildList") {
          // Update the ownership tree according to the mutation record.
          let addedFronts = [];
          let removedFronts = [];
          for (let removed of change.removed) {
            let removedFront = this.get(removed);
            if (!removedFront) {
              console.error("Got a removal of an actor we didn't know about: " +
                removed);
              continue;
            }
            // Remove from the ownership tree
            removedFront.reparent(null);

            // This node is orphaned unless we get it in the 'added' list
            // eventually.
            this._orphaned.add(removedFront);
            removedFronts.push(removedFront);
          }
          for (let added of change.added) {
            let addedFront = this.get(added);
            if (!addedFront) {
              console.error("Got an addition of an actor we didn't know " +
                "about: " + added);
              continue;
            }
            addedFront.reparent(targetFront);

            // The actor is reconnected to the ownership tree, unorphan
            // it.
            this._orphaned.delete(addedFront);
            addedFronts.push(addedFront);
          }

          if (change.singleTextChild) {
            targetFront.singleTextChild =
              types.getType("domnode").read(change.singleTextChild, this);
          } else {
            targetFront.singleTextChild = undefined;
          }

          // Before passing to users, replace the added and removed actor
          // ids with front in the mutation record.
          emittedMutation.added = addedFronts;
          emittedMutation.removed = removedFronts;

          // If this is coming from a DOM mutation, the actor's numChildren
          // was passed in. Otherwise, it is simulated from a frame load or
          // unload, so don't change the front's form.
          if ("numChildren" in change) {
            targetFront._form.numChildren = change.numChildren;
          }
        } else if (change.type === "frameLoad") {
          // Nothing we need to do here, except verify that we don't have any
          // document children, because we should have gotten a documentUnload
          // first.
          for (let child of targetFront.treeChildren()) {
            if (child.nodeType === Ci.nsIDOMNode.DOCUMENT_NODE) {
              console.trace("Got an unexpected frameLoad in the inspector, " +
                "please file a bug on bugzilla.mozilla.org!");
            }
          }
        } else if (change.type === "documentUnload") {
          if (targetFront === this.rootNode) {
            this._createRootNodePromise();
          }

          // We try to give fronts instead of actorIDs, but these fronts need
          // to be destroyed now.
          emittedMutation.target = targetFront.actorID;
          emittedMutation.targetParent = targetFront.parentNode();

          // Release the document node and all of its children, even retained.
          this._releaseFront(targetFront, true);
        } else if (change.type === "unretained") {
          // Retained orphans were force-released without the intervention of
          // client (probably a navigated frame).
          for (let released of change.nodes) {
            let releasedFront = this.get(released);
            this._retainedOrphans.delete(released);
            this._releaseFront(releasedFront, true);
          }
        } else {
          targetFront.updateMutation(change);
        }

        emitMutations.push(emittedMutation);
      }

      if (options.cleanup) {
        for (let node of this._orphaned) {
          // This will move retained nodes to this._retainedOrphans.
          this._releaseFront(node);
        }
        this._orphaned = new Set();
      }

      events.emit(this, "mutations", emitMutations);
    });
  }, {
    impl: "_getMutations"
  }),

  /**
   * Handle the `new-mutations` notification by fetching the
   * available mutation records.
   */
  onMutations: preEvent("new-mutations", function () {
    // Fetch and process the mutations.
    this.getMutations({cleanup: this.autoCleanup}).catch(() => {});
  }),

  isLocal: function () {
    return !!this.conn._transport._serverConnection;
  },

  // XXX hack during transition to remote inspector: get a proper NodeFront
  // for a given local node.  Only works locally.
  frontForRawNode: function (rawNode) {
    if (!this.isLocal()) {
      console.warn("Tried to use frontForRawNode on a remote connection.");
      return null;
    }
    let walkerActor = this.conn._transport._serverConnection
      .getActor(this.actorID);
    if (!walkerActor) {
      throw Error("Could not find client side for actor " + this.actorID);
    }
    let nodeActor = walkerActor._ref(rawNode);

    // Pass the node through a read/write pair to create the client side actor.
    let nodeType = types.getType("domnode");
    let returnNode = nodeType.read(
      nodeType.write(nodeActor, walkerActor), this);
    let top = returnNode;
    let extras = walkerActor.parents(nodeActor, {sameTypeRootTreeItem: true});
    for (let extraActor of extras) {
      top = nodeType.read(nodeType.write(extraActor, walkerActor), this);
    }

    if (top !== this.rootNode) {
      // Imported an already-orphaned node.
      this._orphaned.add(top);
      walkerActor._orphaned
        .add(this.conn._transport._serverConnection.getActor(top.actorID));
    }
    return returnNode;
  },

  removeNode: custom(Task.async(function* (node) {
    let previousSibling = yield this.previousSibling(node);
    let nextSibling = yield this._removeNode(node);
    return {
      previousSibling: previousSibling,
      nextSibling: nextSibling,
    };
  }), {
    impl: "_removeNode"
  }),
});

exports.WalkerFront = WalkerFront;

/**
 * Client side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
var InspectorFront = FrontClassWithSpec(inspectorSpec, {
  initialize: function (client, tabForm) {
    Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.inspectorActor;

    // XXX: This is the first actor type in its hierarchy to use the protocol
    // library, so we're going to self-own on the client side for now.
    this.manage(this);
  },

  destroy: function () {
    delete this.walker;
    Front.prototype.destroy.call(this);
  },

  getWalker: custom(function (options = {}) {
    return this._getWalker(options).then(walker => {
      this.walker = walker;
      return walker;
    });
  }, {
    impl: "_getWalker"
  }),

  getPageStyle: custom(function () {
    return this._getPageStyle().then(pageStyle => {
      // We need a walker to understand node references from the
      // node style.
      if (this.walker) {
        return pageStyle;
      }
      return this.getWalker().then(() => {
        return pageStyle;
      });
    });
  }, {
    impl: "_getPageStyle"
  })
});

exports.InspectorFront = InspectorFront;
