/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { UPDATE_ELEMENT_PICKER_ENABLED } = require("./index");

module.exports = {
  /**
   * Update the state of element picker in animation inspector.
   */
  updateElementPickerEnabled(isEnabled) {
    return {
      type: UPDATE_ELEMENT_PICKER_ENABLED,
      isEnabled,
    };
  }
};
