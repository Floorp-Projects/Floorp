/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_CLASSES,
  UPDATE_CLASS_PANEL_EXPANDED,
} = require("../actions/index");

const INITIAL_CLASS_LIST = {
  // An array of objects containing the CSS class state that is applied to the current
  // element.
  classes: [],
  // Whether or not the class list panel is expanded.
  isClassPanelExpanded: false,
};

const reducers = {
  [UPDATE_CLASSES](classList, { classes }) {
    return {
      ...classList,
      classes: [...classes],
    };
  },

  [UPDATE_CLASS_PANEL_EXPANDED](classList, { isClassPanelExpanded }) {
    return {
      ...classList,
      isClassPanelExpanded,
    };
  },
};

module.exports = function(classList = INITIAL_CLASS_LIST, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return classList;
  }
  return reducer(classList, action);
};
