/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(
  this,
  "nodeConstants",
  "devtools/shared/dom-node-constants"
);

/**
 * Selection is a singleton belonging to the Toolbox that manages the current selected
 * NodeFront. In addition, it provides some helpers about the context of the selected
 * node.
 *
 * API
 *
 *   new Selection()
 *   destroy()
 *   nodeFront (readonly)
 *   setNodeFront(node, origin="unknown")
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
function Selection() {
  EventEmitter.decorate(this);

  // The WalkerFront is dynamic and is always set to the selected NodeFront's WalkerFront.
  this._walker = null;
  // A single node front can be represented twice on the client when the node is a slotted
  // element. It will be displayed once as a direct child of the host element, and once as
  // a child of a slot in the "shadow DOM". The latter is called the slotted version.
  this._isSlotted = false;

  this._onMutations = this._onMutations.bind(this);
  this.setNodeFront = this.setNodeFront.bind(this);
}

Selection.prototype = {
  _onMutations(mutations) {
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

  destroy() {
    this.setWalker();
    this._nodeFront = null;
  },

  /**
   * @param {WalkerFront|null} walker
   */
  setWalker(walker = null) {
    if (this._walker) {
      this._removeWalkerFrontEventListeners(this._walker);
    }

    this._walker = walker;
    if (this._walker) {
      this._setWalkerFrontEventListeners(this._walker);
    }
  },

  /**
   * Set event listeners on the passed walker front
   *
   * @param {WalkerFront} walker
   */
  _setWalkerFrontEventListeners(walker) {
    walker.on("mutations", this._onMutations);
  },

  /**
   * Remove event listeners we previously set on walker front
   *
   * @param {WalkerFront} walker
   */
  _removeWalkerFrontEventListeners(walker) {
    walker.off("mutations", this._onMutations);
  },

  /**
   * Called when a target front is destroyed.
   *
   * @param {TargetFront} front
   * @emits detached-front
   */
  onTargetDestroyed(targetFront) {
    // if the current walker belongs to the target that is destroyed, emit a `detached-front`
    // event so consumers can act accordingly (e.g. in the inspector, another node will be
    // selected)
    if (
      this._walker &&
      !targetFront.isTopLevel &&
      this._walker.targetFront == targetFront
    ) {
      this._removeWalkerFrontEventListeners(this._walker);
      this.emit("detached-front");
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
  setNodeFront(nodeFront, { reason = "unknown", isSlotted = false } = {}) {
    this.reason = reason;

    // If an inlineTextChild text node is being set, then set it's parent instead.
    const parentNode = nodeFront && nodeFront.parentNode();
    if (nodeFront && parentNode && parentNode.inlineTextChild === nodeFront) {
      nodeFront = parentNode;
    }

    if (this._nodeFront == null && nodeFront == null) {
      // Avoid to notify multiple "unselected" events with a null/undefined nodeFront
      // (e.g. once when the webpage start to navigate away from the current webpage,
      // and then again while the new page is being loaded).
      return;
    }

    this.emit("node-front-will-unset");

    this._isSlotted = isSlotted;
    this._nodeFront = nodeFront;

    if (nodeFront) {
      this.setWalker(nodeFront.walkerFront);
    } else {
      this.setWalker();
    }

    this.emit("new-node-front", nodeFront, this.reason);
  },

  get nodeFront() {
    return this._nodeFront;
  },

  isRoot() {
    return (
      this.isNode() && this.isConnected() && this._nodeFront.isDocumentElement
    );
  },

  isNode() {
    return !!this._nodeFront;
  },

  isConnected() {
    let node = this._nodeFront;
    if (!node || node.isDestroyed()) {
      return false;
    }

    while (node) {
      if (node === this._walker.rootNode) {
        return true;
      }
      node = node.parentOrHost();
    }
    return false;
  },

  isHTMLNode() {
    const xhtmlNs = "http://www.w3.org/1999/xhtml";
    return this.isNode() && this.nodeFront.namespaceURI == xhtmlNs;
  },

  isSVGNode() {
    const svgNs = "http://www.w3.org/2000/svg";
    return this.isNode() && this.nodeFront.namespaceURI == svgNs;
  },

  isMathMLNode() {
    const mathmlNs = "http://www.w3.org/1998/Math/MathML";
    return this.isNode() && this.nodeFront.namespaceURI == mathmlNs;
  },

  // Node type

  isElementNode() {
    return (
      this.isNode() && this.nodeFront.nodeType == nodeConstants.ELEMENT_NODE
    );
  },

  isPseudoElementNode() {
    return this.isNode() && this.nodeFront.isPseudoElement;
  },

  isAnonymousNode() {
    return this.isNode() && this.nodeFront.isAnonymous;
  },

  isAttributeNode() {
    return (
      this.isNode() && this.nodeFront.nodeType == nodeConstants.ATTRIBUTE_NODE
    );
  },

  isTextNode() {
    return this.isNode() && this.nodeFront.nodeType == nodeConstants.TEXT_NODE;
  },

  isCDATANode() {
    return (
      this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.CDATA_SECTION_NODE
    );
  },

  isEntityRefNode() {
    return (
      this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.ENTITY_REFERENCE_NODE
    );
  },

  isEntityNode() {
    return (
      this.isNode() && this.nodeFront.nodeType == nodeConstants.ENTITY_NODE
    );
  },

  isProcessingInstructionNode() {
    return (
      this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.PROCESSING_INSTRUCTION_NODE
    );
  },

  isCommentNode() {
    return (
      this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.PROCESSING_INSTRUCTION_NODE
    );
  },

  isDocumentNode() {
    return (
      this.isNode() && this.nodeFront.nodeType == nodeConstants.DOCUMENT_NODE
    );
  },

  /**
   * @returns true if the selection is the <body> HTML element.
   */
  isBodyNode() {
    return (
      this.isHTMLNode() &&
      this.isConnected() &&
      this.nodeFront.nodeName === "BODY"
    );
  },

  /**
   * @returns true if the selection is the <head> HTML element.
   */
  isHeadNode() {
    return (
      this.isHTMLNode() &&
      this.isConnected() &&
      this.nodeFront.nodeName === "HEAD"
    );
  },

  isDocumentTypeNode() {
    return (
      this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.DOCUMENT_TYPE_NODE
    );
  },

  isDocumentFragmentNode() {
    return (
      this.isNode() &&
      this.nodeFront.nodeType == nodeConstants.DOCUMENT_FRAGMENT_NODE
    );
  },

  isNotationNode() {
    return (
      this.isNode() && this.nodeFront.nodeType == nodeConstants.NOTATION_NODE
    );
  },

  isSlotted() {
    return this._isSlotted;
  },

  isShadowRootNode() {
    return this.isNode() && this.nodeFront.isShadowRoot;
  },
};

module.exports = Selection;
