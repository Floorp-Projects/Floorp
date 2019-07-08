/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DISABLE_ALL_PSEUDO_CLASSES,
  SET_PSEUDO_CLASSES,
  TOGGLE_PSEUDO_CLASS,
} = require("../actions/index");
const { PSEUDO_CLASSES } = require("devtools/shared/css/constants");

const INITIAL_PSEUDO_CLASSES = PSEUDO_CLASSES.reduce(
  (accumulator, pseudoClass) => {
    accumulator[pseudoClass] = {
      isChecked: false,
      isDisabled: false,
    };
    return accumulator;
  },
  {}
);

const reducers = {
  [DISABLE_ALL_PSEUDO_CLASSES]() {
    return PSEUDO_CLASSES.reduce((accumulator, pseudoClass) => {
      accumulator[pseudoClass] = {
        isChecked: false,
        isDisabled: true,
      };
      return accumulator;
    }, {});
  },

  [SET_PSEUDO_CLASSES](_, { pseudoClassLocks }) {
    return PSEUDO_CLASSES.reduce((accumulator, pseudoClass) => {
      accumulator[pseudoClass] = {
        isChecked: pseudoClassLocks.includes(pseudoClass),
        isDisabled: false,
      };
      return accumulator;
    }, {});
  },

  [TOGGLE_PSEUDO_CLASS](pseudoClasses, { pseudoClass }) {
    return {
      ...pseudoClasses,
      [pseudoClass]: {
        ...pseudoClasses[pseudoClass],
        isChecked: !pseudoClasses[pseudoClass].isChecked,
      },
    };
  },
};

module.exports = function(pseudoClasses = INITIAL_PSEUDO_CLASSES, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return pseudoClasses;
  }
  return reducer(pseudoClasses, action);
};
