/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  AUDIT,
  AUDITING,
  FILTER_TOGGLE,
  FILTERS,
  RESET,
  SELECT,
} = require("../constants");

/**
 * Initial state definition
 */
function getInitialState() {
  return {
    filters: {
      [FILTERS.CONTRAST]: false,
    },
    auditing: null,
  };
}

function audit(state = getInitialState(), action) {
  switch (action.type) {
    case FILTER_TOGGLE:
      const { filter } = action;
      let { filters } = state;
      const active = !filters[filter];
      filters = {
        ...filters,
        [filter]: active,
      };

      return {
        ...state,
        filters,
      };
    case AUDITING:
      const { auditing } = action;

      return {
        ...state,
        auditing,
      };
    case AUDIT:
      return {
        ...state,
        auditing: null,
      };
    case SELECT:
    case RESET:
      return getInitialState();
    default:
      return state;
  }
}

exports.audit = audit;
