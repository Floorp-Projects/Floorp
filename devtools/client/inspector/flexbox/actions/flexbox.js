/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CLEAR_FLEXBOX,
  UPDATE_FLEXBOX,
  UPDATE_FLEXBOX_COLOR,
  UPDATE_FLEXBOX_HIGHLIGHTED,
} = require("./index");

module.exports = {

  /**
   * Clears the flexbox state by resetting it back to the initial flexbox state.
   */
  clearFlexbox() {
    return {
      type: CLEAR_FLEXBOX,
    };
  },

  /**
   * Updates the flexbox state with the newly selected flexbox.
   */
  updateFlexbox(flexbox) {
    return {
      type: UPDATE_FLEXBOX,
      flexbox,
    };
  },

  /**
   * Update the color used for the flexbox's highlighter.
   *
   * @param  {String} color
   *         The color to use for this nodeFront's flexbox highlighter.
   */
  updateFlexboxColor(color) {
    return {
      type: UPDATE_FLEXBOX_COLOR,
      color,
    };
  },

  /**
   * Updates the flexbox highlighted state.
   *
   * @param  {Boolean} highlighted
   *         Whether or not the flexbox highlighter is highlighting the flexbox.
   */
  updateFlexboxHighlighted(highlighted) {
    return {
      type: UPDATE_FLEXBOX_HIGHLIGHTED,
      highlighted,
    };
  },

};
