/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DISABLE_ALL_PSEUDO_CLASSES,
  SET_PSEUDO_CLASSES,
  TOGGLE_PSEUDO_CLASS,
} = require("./index");

module.exports = {
  /**
   * Disables all the pseudo class checkboxes because the current selection is not an
   * element node.
   */
  disableAllPseudoClasses() {
    return {
      type: DISABLE_ALL_PSEUDO_CLASSES,
    };
  },

  /**
   * Sets the entire pseudo class state with the new list of applied pseudo-class
   * locks.
   *
   * @param  {Array<String>} pseudoClassLocks
   *         Array of all pseudo class locks applied to the current selected element.
   */
  setPseudoClassLocks(pseudoClassLocks) {
    return {
      type: SET_PSEUDO_CLASSES,
      pseudoClassLocks,
    };
  },

  /**
   * Toggles on or off the given pseudo class value for the current selected element.
   *
   * @param  {String} pseudoClass
   *         The pseudo class value to toggle on or off.
   */
  togglePseudoClass(pseudoClass) {
    return {
      type: TOGGLE_PSEUDO_CLASS,
      pseudoClass,
    };
  },
};
