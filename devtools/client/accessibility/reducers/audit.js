/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  accessibility: { AUDIT_TYPE },
} = require("devtools/shared/constants");
const {
  AUDIT,
  AUDITING,
  AUDIT_PROGRESS,
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
      [FILTERS.ALL]: false,
      [FILTERS.CONTRAST]: false,
      [FILTERS.TEXT_LABEL]: false,
    },
    auditing: [],
    progress: null,
  };
}

/**
 * State with all filters active.
 */
function allActiveFilters() {
  return {
    [FILTERS.ALL]: true,
    [FILTERS.CONTRAST]: true,
    [FILTERS.TEXT_LABEL]: true,
  };
}

function audit(state = getInitialState(), action) {
  switch (action.type) {
    case FILTER_TOGGLE:
      const { filter } = action;
      let { filters } = state;
      const isToggledToActive = !filters[filter];

      if (filter === FILTERS.NONE) {
        filters = getInitialState().filters;
      } else if (filter === FILTERS.ALL) {
        filters = isToggledToActive
          ? allActiveFilters()
          : getInitialState().filters;
      } else {
        filters = {
          ...filters,
          [filter]: isToggledToActive,
        };

        const allAuditTypesActive = Object.values(AUDIT_TYPE)
          .filter(filterKey => filters.hasOwnProperty(filterKey))
          .every(filterKey => filters[filterKey]);
        if (isToggledToActive && !filters[FILTERS.ALL] && allAuditTypesActive) {
          filters[FILTERS.ALL] = true;
        } else if (!isToggledToActive && filters[FILTERS.ALL]) {
          filters[FILTERS.ALL] = false;
        }
      }

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
        auditing: getInitialState().auditing,
        progress: null,
      };
    case AUDIT_PROGRESS:
      const { progress } = action;

      return {
        ...state,
        progress,
      };
    case SELECT:
    case RESET:
      return getInitialState();
    default:
      return state;
  }
}

exports.audit = audit;
