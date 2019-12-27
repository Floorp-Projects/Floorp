/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { UPDATE_CLASSES, UPDATE_CLASS_PANEL_EXPANDED } = require("./index");

module.exports = {
  /**
   * Updates the entire class list state with the new list of classes.
   *
   * @param  {Array<Object>} classes
   *         Array of CSS classes object applied to the element.
   */
  updateClasses(classes) {
    return {
      type: UPDATE_CLASSES,
      classes,
    };
  },

  /**
   * Updates whether or not the class list panel is expanded.
   *
   * @param  {Boolean} isClassPanelExpanded
   *         Whether or not the class list panel is expanded.
   */
  updateClassPanelExpanded(isClassPanelExpanded) {
    return {
      type: UPDATE_CLASS_PANEL_EXPANDED,
      isClassPanelExpanded,
    };
  },
};
