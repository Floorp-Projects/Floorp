/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  RESET_EDITOR,
  UPDATE_AXIS_VALUE,
  UPDATE_EDITOR_VISIBILITY,
  UPDATE_EDITOR_STATE,
} = require("./index");

module.exports = {

  resetFontEditor() {
    return {
      type: RESET_EDITOR,
    };
  },

  toggleFontEditor(isVisible, selector = "") {
    return {
      type: UPDATE_EDITOR_VISIBILITY,
      isVisible,
      selector,
    };
  },

  updateAxis(axis, value) {
    return {
      type: UPDATE_AXIS_VALUE,
      axis,
      value,
    };
  },

  updateFontEditor(fonts, properties = {}) {
    return {
      type: UPDATE_EDITOR_STATE,
      fonts,
      properties,
    };
  },

};
