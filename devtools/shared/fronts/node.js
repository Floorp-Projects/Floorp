/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");
const {
  FrontClassWithSpec,
  types,
  registerFront,
} = require("devtools/shared/protocol.js");
const { nodeSpec, nodeListSpec } = require("devtools/shared/specs/node");
const { SimpleStringFront } = require("devtools/shared/fronts/string");

loader.lazyRequireGetter(
  this,
  "nodeConstants",
  "devtools/shared/dom-node-constants"
);

loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/shared/fronts/targets/browsing-context",
  true
);

const HIDDEN_CLASS = "__fx-devtools-hide-shortcut__";

/**
 * Client side of a node list as returned by querySelectorAll()
 */
class NodeListFront extends FrontClassWithSpec(nodeListSpec) {
  marshallPool() {
    return this.parent();
  }

  // Update the object given a form representation off the wire.
  form(json) {
    this.length = json.length;
  }

  item(index) {
    return super.item(index).then(response => {
      return response.node;
    });
  }

  items(start, end) {
    return super.items(start, end).then(response => {
      return response.nodes;
    });
  }
}

exports.NodeListFront = NodeListFront;
registerFront(NodeListFront);

/**
 * Convenience API for building a list of attribute modifications
 * for the `modifyAttributes` request.
 */
class AttributeModificationList {
  constructor(node) {
    this.node = node;
    this.modifications = [];
  }

  apply() {
    const ret = this.node.modifyAttributes(this.modifications);
    return ret;
  }

  destroy() {
    this.node = null;
    this.modification = null;
  }

  setAttributeNS(ns, name, value) {
    this.modifications.push({
      attributeNamespace: ns,
      attributeName: name,
      newValue: value,
    });
  }

  setAttribute(name, value) {
    this.setAttributeNS(undefined, name, value);
  }

  removeAttributeNS(ns, name) {
    this.setAttributeNS(ns, name, undefined);
  }

  removeAttribute(name) {
    this.setAttributeNS(undefined, name, undefined);
  }
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
class NodeFront extends FrontClassWithSpec(nodeSpec) {
  constructor(conn, targetFront, parentFront) {
    super(conn, targetFront, parentFront);
    // The parent node
    this._parent = null;
    // The first child of this node.
    this._child = null;
    // The next sibling of this node.
    this._next = null;
    // The previous sibling of this node.
    this._prev = null;
  }

  /**
   * Destroy a node front.  The node must have been removed from the
   * ownership tree before this is called, unless the whole walker front
   * is being destroyed.
   */
  destroy() {
    super.destroy();
  }

  // Update the object given a form representation off the wire.
  form(form, ctx) {
    // backward-compatibility: shortValue indicates we are connected to old server
    if (form.shortValue) {
      // If the value is not complete, set nodeValue to null, it will be fetched
      // when calling getNodeValue()
      form.nodeValue = form.incompleteValue ? null : form.shortValue;
    }

    // Shallow copy of the form.  We could just store a reference, but
    // eventually we'll want to update some of the data.
    this._form = Object.assign({}, form);
    this._form.attrs = this._form.attrs ? this._form.attrs.slice() : [];

    if (form.parent) {
      // Get the owner actor for this actor (the walker), and find the
      // parent node of this actor from it, creating a standin node if
      // necessary.
      const parentNodeFront = ctx
        .marshallPool()
        .ensureDOMNodeFront(form.parent);
      this.reparent(parentNodeFront);
    }

    if (form.host) {
      this.host = ctx.marshallPool().ensureDOMNodeFront(form.host);
    }

    if (form.inlineTextChild) {
      this.inlineTextChild = types
        .getType("domnode")
        .read(form.inlineTextChild, ctx);
    } else {
      this.inlineTextChild = undefined;
    }
  }

  /**
   * Returns the parent NodeFront for this NodeFront.
   */
  parentNode() {
    return this._parent;
  }

  /**
   * Returns the NodeFront corresponding to the parentNode of this NodeFront, or the
   * NodeFront corresponding to the host element for shadowRoot elements.
   */
  parentOrHost() {
    return this.isShadowRoot ? this.host : this._parent;
  }

  /**
   * Process a mutation entry as returned from the walker's `getMutations`
   * request.  Only tries to handle changes of the node's contents
   * themselves (character data and attribute changes), the walker itself
   * will keep the ownership tree up to date.
   */
  updateMutation(change) {
    if (change.type === "attributes") {
      // We'll need to lazily reparse the attributes after this change.
      this._attrMap = undefined;

      // Update any already-existing attributes.
      let found = false;
      for (let i = 0; i < this.attributes.length; i++) {
        const attr = this.attributes[i];
        if (
          attr.name == change.attributeName &&
          attr.namespace == change.attributeNamespace
        ) {
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
          value: change.newValue,
        });
      }
    } else if (change.type === "characterData") {
      this._form.nodeValue = change.newValue;
    } else if (change.type === "pseudoClassLock") {
      this._form.pseudoClassLocks = change.pseudoClassLocks;
    } else if (change.type === "events") {
      this._form.hasEventListeners = change.hasEventListeners;
    } else if (change.type === "mutationBreakpoint") {
      this._form.mutationBreakpoints = change.mutationBreakpoints;
    }
  }

  // Some accessors to make NodeFront feel more like a Node

  get id() {
    return this.getAttribute("id");
  }

  get nodeType() {
    return this._form.nodeType;
  }
  get namespaceURI() {
    return this._form.namespaceURI;
  }
  get nodeName() {
    return this._form.nodeName;
  }
  get displayName() {
    const { displayName, nodeName } = this._form;

    // Keep `nodeName.toLowerCase()` for backward compatibility
    return displayName || nodeName.toLowerCase();
  }
  get doctypeString() {
    return (
      "<!DOCTYPE " +
      this._form.name +
      (this._form.publicId ? ' PUBLIC "' + this._form.publicId + '"' : "") +
      (this._form.systemId ? ' "' + this._form.systemId + '"' : "") +
      ">"
    );
  }

  get baseURI() {
    return this._form.baseURI;
  }

  get className() {
    return this.getAttribute("class") || "";
  }

  get hasChildren() {
    return this._form.numChildren > 0;
  }
  get numChildren() {
    return this._form.numChildren;
  }
  get remoteFrame() {
    return this._form.remoteFrame;
  }
  get hasEventListeners() {
    return this._form.hasEventListeners;
  }

  get isMarkerPseudoElement() {
    return this._form.isMarkerPseudoElement;
  }
  get isBeforePseudoElement() {
    return this._form.isBeforePseudoElement;
  }
  get isAfterPseudoElement() {
    return this._form.isAfterPseudoElement;
  }
  get isPseudoElement() {
    return (
      this.isBeforePseudoElement ||
      this.isAfterPseudoElement ||
      this.isMarkerPseudoElement
    );
  }
  get isAnonymous() {
    return this._form.isAnonymous;
  }
  get isInHTMLDocument() {
    return this._form.isInHTMLDocument;
  }
  get tagName() {
    return this.nodeType === nodeConstants.ELEMENT_NODE ? this.nodeName : null;
  }

  get isDocumentElement() {
    return !!this._form.isDocumentElement;
  }

  get isShadowRoot() {
    return this._form.isShadowRoot;
  }

  get shadowRootMode() {
    return this._form.shadowRootMode;
  }

  get isShadowHost() {
    return this._form.isShadowHost;
  }

  get customElementLocation() {
    return this._form.customElementLocation;
  }

  get isDirectShadowHostChild() {
    return this._form.isDirectShadowHostChild;
  }

  // doctype properties
  get name() {
    return this._form.name;
  }
  get publicId() {
    return this._form.publicId;
  }
  get systemId() {
    return this._form.systemId;
  }

  getAttribute(name) {
    const attr = this._getAttribute(name);
    return attr ? attr.value : null;
  }
  hasAttribute(name) {
    this._cacheAttributes();
    return name in this._attrMap;
  }

  get hidden() {
    const cls = this.getAttribute("class");
    return cls && cls.indexOf(HIDDEN_CLASS) > -1;
  }

  get attributes() {
    return this._form.attrs;
  }

  get mutationBreakpoints() {
    return this._form.mutationBreakpoints;
  }

  get pseudoClassLocks() {
    return this._form.pseudoClassLocks || [];
  }
  hasPseudoClassLock(pseudo) {
    return this.pseudoClassLocks.some(locked => locked === pseudo);
  }

  get displayType() {
    return this._form.displayType;
  }

  get isDisplayed() {
    return this._form.isDisplayed;
  }

  get isScrollable() {
    return this._form.isScrollable;
  }

  get isTreeDisplayed() {
    let parent = this;
    while (parent) {
      if (!parent.isDisplayed) {
        return false;
      }
      parent = parent.parentNode();
    }
    return true;
  }

  get walkerFront() {
    return this.parentFront;
  }

  getNodeValue() {
    // backward-compatibility: if nodevalue is null and shortValue is defined, the actual
    // value of the node needs to be fetched on the server.
    if (this._form.nodeValue === null && this._form.shortValue) {
      return super.getNodeValue();
    }

    const str = this._form.nodeValue || "";
    return promise.resolve(new SimpleStringFront(str));
  }

  /**
   * Return a new AttributeModificationList for this node.
   */
  startModifyingAttributes() {
    return new AttributeModificationList(this);
  }

  _cacheAttributes() {
    if (typeof this._attrMap != "undefined") {
      return;
    }
    this._attrMap = {};
    for (const attr of this.attributes) {
      this._attrMap[attr.name] = attr;
    }
  }

  _getAttribute(name) {
    this._cacheAttributes();
    return this._attrMap[name] || undefined;
  }

  /**
   * Set this node's parent.  Note that the children saved in
   * this tree are unordered and incomplete, so shouldn't be used
   * instead of a `children` request.
   */
  reparent(parent) {
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
  }

  /**
   * Return all the known children of this node.
   */
  treeChildren() {
    const ret = [];
    for (let child = this._child; child != null; child = child._next) {
      ret.push(child);
    }
    return ret;
  }

  /**
   * Do we use a local target?
   * Useful to know if a rawNode is available or not.
   *
   * This will, one day, be removed. External code should
   * not need to know if the target is remote or not.
   */
  isLocalToBeDeprecated() {
    return !!this.conn._transport._serverConnection;
  }

  /**
   * Get a Node for the given node front.  This only works locally,
   * and is only intended as a stopgap during the transition to the remote
   * protocol.  If you depend on this you're likely to break soon.
   */
  rawNode(rawNode) {
    if (!this.isLocalToBeDeprecated()) {
      console.warn("Tried to use rawNode on a remote connection.");
      return null;
    }
    const { DebuggerServer } = require("devtools/server/debugger-server");
    const actor = DebuggerServer.searchAllConnectionsForActor(this.actorID);
    if (!actor) {
      // Can happen if we try to get the raw node for an already-expired
      // actor.
      return null;
    }
    return actor.rawNode;
  }

  async connectToRemoteFrame() {
    if (this._remoteFrameTarget) {
      return this._remoteFrameTarget;
    }
    // First get the target actor form of this remote frame element
    const form = await super.connectToRemoteFrame();
    // Build the related Target object
    this._remoteFrameTarget = new BrowsingContextTargetFront(this.conn);
    this._remoteFrameTarget.actorID = form.actor;
    this._remoteFrameTarget.form(form);
    this._remoteFrameTarget.manage(this._remoteFrameTarget);
    return this._remoteFrameTarget;
  }
}

exports.NodeFront = NodeFront;
registerFront(NodeFront);
