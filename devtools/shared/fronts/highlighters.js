/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { FrontClassWithSpec, custom } = require("devtools/shared/protocol");
const flags = require("devtools/shared/flags");
const {
  customHighlighterSpec,
  highlighterSpec,
} = require("devtools/shared/specs/highlighters");

const HighlighterFront = FrontClassWithSpec(highlighterSpec, {
  isNodeFrontHighlighted: false,
  // Update the object given a form representation off the wire.
  form: function(json) {
    this.actorID = json.actor;
    // FF42+ HighlighterActors starts exposing custom form, with traits object
    this.traits = json.traits || {};
  },

  pick: custom(function(doFocus) {
    if (doFocus && this.pickAndFocus) {
      return this.pickAndFocus();
    }
    return this._pick();
  }, {
    impl: "_pick",
  }),

  /**
   * Show the box model highlighter on a node in the content page.
   * The node needs to be a NodeFront, as defined by the inspector actor
   * @see devtools/server/actors/inspector/inspector.js
   * @param {NodeFront} nodeFront The node to highlight
   * @param {Object} options
   * @return A promise that resolves when the node has been highlighted
   */
  highlight: async function(nodeFront, options = {}) {
    if (!nodeFront) {
      return;
    }

    this.isNodeFrontHighlighted = true;
    await this.showBoxModel(nodeFront, options);
    this.emit("node-highlight", nodeFront);
  },

  /**
   * Hide the highlighter.
   * @param {Boolean} forceHide Only really matters in test mode (when
   * flags.testing is true). In test mode, hovering over several nodes
   * in the markup view doesn't hide/show the highlighter to ease testing. The
   * highlighter stays visible at all times, except when the mouse leaves the
   * markup view, which is when this param is passed to true
   * @return a promise that resolves when the highlighter is hidden
   */
  unhighlight: async function(forceHide = false) {
    forceHide = forceHide || !flags.testing;

    if (this.isNodeFrontHighlighted && forceHide) {
      this.isNodeFrontHighlighted = false;
      await this.hideBoxModel();
    }

    this.emit("node-unhighlight");
  },
});

exports.HighlighterFront = HighlighterFront;

const CustomHighlighterFront = FrontClassWithSpec(customHighlighterSpec, {
  _isShown: false,

  show: custom(function(...args) {
    this._isShown = true;
    return this._show(...args);
  }, {
    impl: "_show",
  }),

  hide: custom(function() {
    this._isShown = false;
    return this._hide();
  }, {
    impl: "_hide",
  }),

  isShown: function() {
    return this._isShown;
  },
});

exports.CustomHighlighterFront = CustomHighlighterFront;
