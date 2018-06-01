/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nodeConstants = require("devtools/shared/dom-node-constants");
var EventEmitter = require("devtools/shared/event-emitter");

/**
 * API
 *
 *   new Selection(walker=null)
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
 *   "new-node-front" when the inner node changed
 *   "attribute-changed" when an attribute is changed
 *   "detached-front" when the node (or one of its parents) is removed from
 *   the document
 *   "reparented" when the node (or one of its parents) is moved under
 *   a different node
 */

/**
 * A Selection object. Hold a reference to a node.
 * Includes some helpers, fire some helpful events.
 */
function Selection(walker) {
  EventEmitter.decorate(this);

  // A single node front can be represented twice on the client when the node is a slotted
  // element. It will be displayed once as a direct child of the host element, and once as
  // a child of a slot in the "shadow DOM". The latter is called the slotted version.
  this._isSlotted = false;

  this._onMutations = this._onMutations.bind(this);
  this.setNodeFront = this.setNodeFront.bind(this);
  this.setWalker(walker);
}

exports.Selection = Selection;

Selection.prototype = {
  _walker: null,

  _onMutations: function(mutations) {
    let attributeChange = false;
    let pseudoChange = false;
    let detached = false;
    let parentNode = null;

    for (const m of mutations) {
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
      this.emit("detached-front", parentNode);
    }
  },

  destroy: function() {
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

  /**
   * Update the currently selected node-front.
   *
   * @param {NodeFront} nodeFront
   *        The NodeFront being selected.
   * @param {Object} (optional)
   *        - {String} reason: Reason that triggered the selection, will be fired with
   *          the "new-node-front" event.
   *        - {Boolean} isSlotted: Is the selection representing the slotted version of
   *          the node.
   */
  setNodeFront: function(nodeFront, { reason = "unknown", isSlotted = false} = {}) {
    this.reason = reason;

    // If an inlineTextChild text node is being set, then set it's parent instead.
    const parentNode = nodeFront && nodeFront.parentNode();
    if (nodeFront && parentNode && parentNode.inlineTextChild === nodeFront) {
      nodeFront = parentNode;
    }

    this._isSlotted = isSlotted;
    this._nodeFront = nodeFront;
    this.emit("new-node-front", nodeFront, this.reason);
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
    return !!this._nodeFront;
  },

  isConnected: function() {
    let node = this._nodeFront;
    if (!node || !node.actorID) {
      return false;
    }

    while (node) {
      if (node === this._walker.rootNode) {
        return true;
      }
      node = node.parentNode();
    }
    return false;
  },

  isHTMLNode: function() {
    const xhtmlNs = "http://www.w3.org/1999/xhtml";
    return this.isNode() && this.nodeFront.namespaceURI == xhtmlNs;
  },

  // Node type

  isElementNode: function() {
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.ELEMENT_NODE;
  },

  isPseudoElementNode: function() {
    return this.isNode() && this.nodeFront.isPseudoElement;
  },

  isAnonymousNode: function() {
    return this.isNode() && this.nodeFront.isAnonymous;
  },

  isAttributeNode: function() {
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.ATTRIBUTE_NODE;
  },

  isTextNode: function() {
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.TEXT_NODE;
  },

  isCDATANode: function() {
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.CDATA_SECTION_NODE;
  },

  isEntityRefNode: function() {
    return this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.ENTITY_REFERENCE_NODE;
  },

  isEntityNode: function() {
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.ENTITY_NODE;
  },

  isProcessingInstructionNode: function() {
    return this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.PROCESSING_INSTRUCTION_NODE;
  },

  isCommentNode: function() {
    return this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.PROCESSING_INSTRUCTION_NODE;
  },

  isDocumentNode: function() {
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.DOCUMENT_NODE;
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
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.DOCUMENT_TYPE_NODE;
  },

  isDocumentFragmentNode: function() {
    return this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.DOCUMENT_FRAGMENT_NODE;
  },

  isNotationNode: function() {
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.NOTATION_NODE;
  },

  isSlotted: function() {
    return this._isSlotted;
  },
};
