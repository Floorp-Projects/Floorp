/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const flags = require("devtools/shared/flags");
const {
  customHighlighterSpec,
  highlighterSpec,
} = require("devtools/shared/specs/highlighters");

class HighlighterFront extends FrontClassWithSpec(highlighterSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.isNodeFrontHighlighted = false;
    this.isPicking = false;
  }

  // Update the object given a form representation off the wire.
  form(json) {
    this.actorID = json.actor;
    // FF42+ HighlighterActors starts exposing custom form, with traits object
    this.traits = json.traits || {};
  }

  /**
   * Start the element picker on the debuggee target.
   * @param {Boolean} doFocus - Optionally focus the content area once the picker is
   *                            activated.
   * @return promise that resolves when the picker has started or immediately
   * if it is already started
   */
  pick(doFocus) {
    if (this.isPicking) {
      return null;
    }
    this.isPicking = true;
    if (doFocus && super.pickAndFocus) {
      return super.pickAndFocus();
    }
    return super.pick();
  }

  /**
   * Stop the element picker.
   * @return promise that resolves when the picker has stopped or immediately
   * if it is already stopped
   */
  cancelPick() {
    if (!this.isPicking) {
      return Promise.resolve();
    }
    this.isPicking = false;
    return super.cancelPick();
  }

  /**
   * Show the box model highlighter on a node in the content page.
   * The node needs to be a NodeFront, as defined by the inspector actor
   * @see devtools/server/actors/inspector/inspector.js
   * @param {NodeFront} nodeFront The node to highlight
   * @param {Object} options
   * @return A promise that resolves when the node has been highlighted
   */
  async highlight(nodeFront, options = {}) {
    if (!nodeFront) {
      return;
    }

    this.isNodeFrontHighlighted = true;
    await this.showBoxModel(nodeFront, options);
    this.emit("node-highlight", nodeFront);
  }

  /**
   * Hide the highlighter.
   * @param {Boolean} forceHide Only really matters in test mode (when
   * flags.testing is true). In test mode, hovering over several nodes
   * in the markup view doesn't hide/show the highlighter to ease testing. The
   * highlighter stays visible at all times, except when the mouse leaves the
   * markup view, which is when this param is passed to true
   * @return a promise that resolves when the highlighter is hidden
   */
  async unhighlight(forceHide = false) {
    forceHide = forceHide || !flags.testing;

    if (this.isNodeFrontHighlighted && forceHide) {
      this.isNodeFrontHighlighted = false;
      await this.hideBoxModel();
    }

    this.emit("node-unhighlight");
  }
}

exports.HighlighterFront = HighlighterFront;
registerFront(HighlighterFront);

class CustomHighlighterFront extends FrontClassWithSpec(customHighlighterSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this._isShown = false;
  }

  show(...args) {
    this._isShown = true;
    return super.show(...args);
  }

  hide() {
    this._isShown = false;
    return super.hide();
  }

  isShown() {
    return this._isShown;
  }
}

exports.CustomHighlighterFront = CustomHighlighterFront;
registerFront(CustomHighlighterFront);
