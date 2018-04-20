/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  APPLY_FONT_VARIATION_INSTANCE,
  RESET_EDITOR,
  UPDATE_AXIS_VALUE,
  UPDATE_CUSTOM_INSTANCE,
  UPDATE_EDITOR_VISIBILITY,
  UPDATE_EDITOR_STATE,
} = require("./index");

module.exports = {

  resetFontEditor() {
    return {
      type: RESET_EDITOR,
    };
  },

  applyInstance(name, values) {
    return {
      type: APPLY_FONT_VARIATION_INSTANCE,
      name,
      values,
    };
  },

  toggleFontEditor(isVisible, selector = "") {
    return {
      type: UPDATE_EDITOR_VISIBILITY,
      isVisible,
      selector,
    };
  },

  updateCustomInstance() {
    return {
      type: UPDATE_CUSTOM_INSTANCE,
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
