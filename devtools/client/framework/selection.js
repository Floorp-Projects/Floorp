/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, Ci } = require("chrome");
const { getRootBindingParent } = require("devtools/shared/layout/utils");
var EventEmitter = require("devtools/shared/event-emitter");

/**
 * API
 *
 *   new Selection(walker=null, node=null, track={attributes,detached});
 *   destroy()
 *   node (readonly)
 *   setNode(node, origin="unknown")
 *
 * Helpers:
 *
 *   window
 *   document
 *   isRoot()
 *   isNode()
 *   isHTMLNode()
 *
 * Check the nature of the node:
 *
 *   isElementNode()
 *   isAttributeNode()
 *   isTextNode()
 *   isCDATANode()
 *   isEntityRefNode()
 *   isEntityNode()
 *   isProcessingInstructionNode()
 *   isCommentNode()
 *   isDocumentNode()
 *   isDocumentTypeNode()
 *   isDocumentFragmentNode()
 *   isNotationNode()
 *
 * Events:
 *   "new-node" when the inner node changed
 *   "before-new-node" when the inner node is set to change
 *   "attribute-changed" when an attribute is changed (only if tracked)
 *   "detached" when the node (or one of its parents) is removed from the document (only if tracked)
 *   "reparented" when the node (or one of its parents) is moved under a different node (only if tracked)
 */

/**
 * A Selection object. Hold a reference to a node.
 * Includes some helpers, fire some helpful events.
 *
 * @param node Inner node.
 *    Can be null. Can be (un)set in the future via the "node" property;
 * @param trackAttribute Tell if events should be fired when the attributes of
 *    the node change.
 *
 */
function Selection(walker, node=null, track={attributes:true,detached:true}) {
  EventEmitter.decorate(this);

  this._onMutations = this._onMutations.bind(this);
  this.track = track;
  this.setWalker(walker);
  this.setNode(node);
}

exports.Selection = Selection;

Selection.prototype = {
  _walker: null,
  _node: null,

  _onMutations: function(mutations) {
    let attributeChange = false;
    let pseudoChange = false;
    let detached = false;
    let parentNode = null;

    for (let m of mutations) {
      if (!attributeChange && m.type == "attributes") {
        attributeChange = true;
      }
      if (m.type == "childList") {
        if (!detached && !this.isConnected()) {
          if (this.isNode()) {
            parentNode = m.target;
          }
          detached = true;
        }
      }
      if (m.type == "pseudoClassLock") {
        pseudoChange = true;
      }
    }

    // Fire our events depending on what changed in the mutations array
    if (attributeChange) {
      this.emit("attribute-changed");
    }
    if (pseudoChange) {
      this.emit("pseudoclass");
    }
    if (detached) {
      let rawNode = null;
      if (parentNode && parentNode.isLocal_toBeDeprecated()) {
        rawNode = parentNode.rawNode();
      }

      this.emit("detached", rawNode, null);
      this.emit("detached-front", parentNode);
    }
  },

  destroy: function() {
    this.setNode(null);
    this.setWalker(null);
  },

  setWalker: function(walker) {
    if (this._walker) {
      this._walker.off("mutations", this._onMutations);
    }
    this._walker = walker;
    if (this._walker) {
      this._walker.on("mutations", this._onMutations);
    }
  },

  // Not remote-safe
  setNode: function(value, reason="unknown") {
    if (value) {
      value = this._walker.frontForRawNode(value);
    }
    this.setNodeFront(value, reason);
  },

  // Not remote-safe
  get node() {
    return this._node;
  },

  // Not remote-safe
  get window() {
    if (this.isNode()) {
      return this.node.ownerDocument.defaultView;
    }
    return null;
  },

  // Not remote-safe
  get document() {
    if (this.isNode()) {
      return this.node.ownerDocument;
    }
    return null;
  },

  setNodeFront: function(value, reason="unknown") {
    this.reason = reason;

    // We used to return here if the node had not changed but we now need to
    // set the node even if it is already set otherwise it is not possible to
    // e.g. highlight the same node twice.
    let rawValue = null;
    if (value && value.isLocal_toBeDeprecated()) {
      rawValue = value.rawNode();
    }
    this.emit("before-new-node", rawValue, reason);
    this.emit("before-new-node-front", value, reason);
    let previousNode = this._node;
    let previousFront = this._nodeFront;
    this._node = rawValue;
    this._nodeFront = value;
    this.emit("new-node", previousNode, this.reason);
    this.emit("new-node-front", value, this.reason);
  },

  get documentFront() {
    return this._walker.document(this._nodeFront);
  },

  get nodeFront() {
    return this._nodeFront;
  },

  isRoot: function() {
    return this.isNode() &&
           this.isConnected() &&
           this._nodeFront.isDocumentElement;
  },

  isNode: function() {
    if (!this._nodeFront) {
      return false;
    }

    // As long as tools are still accessing node.rawNode(),
    // this needs to stay here.
    if (this._node && Cu.isDeadWrapper(this._node)) {
      return false;
    }

    return true;
  },

  isLocal: function() {
    return !!this._node;
  },

  isConnected: function() {
    let node = this._nodeFront;
    if (!node || !node.actorID) {
      return false;
    }

    // As long as there are still tools going around
    // accessing node.rawNode, this needs to stay.
    let rawNode = null;
    if (node.isLocal_toBeDeprecated()) {
      rawNode = node.rawNode();
    }
    if (rawNode) {
      try {
        let doc = this.document;
        if (doc && doc.defaultView) {
          let docEl = doc.documentElement;
          let bindingParent = getRootBindingParent(rawNode);

          if (docEl.contains(bindingParent)) {
            return true;
          }
        }
      } catch (e) {
        // "can't access dead object" error
      }
      return false;
    }

    while(node) {
      if (node === this._walker.rootNode) {
        return true;
      }
      node = node.parentNode();
    };
    return false;
  },

  isHTMLNode: function() {
    let xhtml_ns = "http://www.w3.org/1999/xhtml";
    return this.isNode() && this.nodeFront.namespaceURI == xhtml_ns;
  },

  // Node type

  isElementNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.ELEMENT_NODE;
  },

  isPseudoElementNode: function() {
    return this.isNode() && this.nodeFront.isPseudoElement;
  },

  isAnonymousNode: function() {
    return this.isNode() && this.nodeFront.isAnonymous;
  },

  isAttributeNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.ATTRIBUTE_NODE;
  },

  isTextNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.TEXT_NODE;
  },

  isCDATANode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.CDATA_SECTION_NODE;
  },

  isEntityRefNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.ENTITY_REFERENCE_NODE;
  },

  isEntityNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.ENTITY_NODE;
  },

  isProcessingInstructionNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.PROCESSING_INSTRUCTION_NODE;
  },

  isCommentNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.PROCESSING_INSTRUCTION_NODE;
  },

  isDocumentNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE;
  },

  /**
   * @returns true if the selection is the <body> HTML element.
   */
  isBodyNode: function() {
    return this.isHTMLNode() &&
           this.isConnected() &&
           this.nodeFront.nodeName === "BODY";
  },

  /**
   * @returns true if the selection is the <head> HTML element.
   */
  isHeadNode: function() {
    return this.isHTMLNode() &&
           this.isConnected() &&
           this.nodeFront.nodeName === "HEAD";
  },

  isDocumentTypeNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.DOCUMENT_TYPE_NODE;
  },

  isDocumentFragmentNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.DOCUMENT_FRAGMENT_NODE;
  },

  isNotationNode: function() {
    return this.isNode() && this.nodeFront.nodeType == Ci.nsIDOMNode.NOTATION_NODE;
  },
};
