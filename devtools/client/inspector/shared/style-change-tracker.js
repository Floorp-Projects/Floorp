/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const WalkerEventListener = require("devtools/client/inspector/shared/walker-event-listener");

/**
 * The InspectorStyleChangeTracker simply emits an event when it detects any changes in
 * the page that may cause the current inspector selection to have different style applied
 * to it.
 * It currently tracks:
 * - markup mutations, because they may cause different CSS rules to apply to the current
 *   node.
 * - window resize, because they may cause media query changes and therefore also
 *   different CSS rules to apply to the current node.
 */
class InspectorStyleChangeTracker {
  constructor(inspector) {
    this.selection = inspector.selection;

    this.onMutations = this.onMutations.bind(this);
    this.onResized = this.onResized.bind(this);

    this.walkerEventListener = new WalkerEventListener(inspector, {
      mutations: this.onMutations,
      resize: this.onResized,
    });

    EventEmitter.decorate(this);
  }

  destroy() {
    this.walkerEventListener.destroy();
    this.walkerEventListener = null;
    this.selection = null;
  }

  /**
   * When markup mutations occur, if an attribute of the selected node, one of its
   * ancestors or siblings changes, we need to consider this as potentially causing a
   * style change for the current node.
   */
  onMutations(mutations) {
    const canMutationImpactCurrentStyles = ({
      type,
      target: mutationTarget,
    }) => {
      // Only attributes mutations are interesting here.
      if (type !== "attributes") {
        return false;
      }

      // Is the mutation on the current selected node?
      const currentNode = this.selection.nodeFront;
      if (mutationTarget === currentNode) {
        return true;
      }

      // Is the mutation on one of the current selected node's siblings?
      // We can't know the order of nodes on the client-side without calling
      // walker.children, so don't attempt to check the previous or next element siblings.
      // It's good enough to know that one sibling changed.
      let parent = currentNode.parentNode();
      const siblings = parent.treeChildren();
      if (siblings.includes(mutationTarget)) {
        return true;
      }

      // Is the mutation on one of the current selected node's parents?
      while (parent) {
        if (mutationTarget === parent) {
          return true;
        }
        parent = parent.parentNode();
      }

      return false;
    };

    for (const mutation of mutations) {
      if (canMutationImpactCurrentStyles(mutation)) {
        this.emit("style-changed");
        break;
      }
    }
  }

  /**
   * When the window gets resized, this may cause media-queries to match, and we therefore
   * need to consider this as a style change for the current node.
   */
  onResized() {
    this.emit("style-changed");
  }
}

module.exports = InspectorStyleChangeTracker;
