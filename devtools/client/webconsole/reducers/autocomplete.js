/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  AUTOCOMPLETE_CLEAR,
  AUTOCOMPLETE_DATA_RECEIVE,
  AUTOCOMPLETE_PENDING_REQUEST,
  AUTOCOMPLETE_RETRIEVE_FROM_CACHE,
} = require("devtools/client/webconsole/constants");

function getDefaultState() {
  return Object.freeze({
    cache: null,
    matches: [],
    matchProp: null,
    isElementAccess: false,
    pendingRequestId: null,
  });
}

function autocomplete(state = getDefaultState(), action) {
  switch (action.type) {
    case AUTOCOMPLETE_CLEAR:
      return getDefaultState();
    case AUTOCOMPLETE_RETRIEVE_FROM_CACHE:
      return autoCompleteRetrieveFromCache(state, action);
    case AUTOCOMPLETE_PENDING_REQUEST:
      return {
        ...state,
        cache: null,
        pendingRequestId: action.id,
      };
    case AUTOCOMPLETE_DATA_RECEIVE:
      if (action.id !== state.pendingRequestId) {
        return state;
      }

      if (action.data.matches === null) {
        return getDefaultState();
      }

      return {
        ...state,
        cache: {
          input: action.input,
          frameActorId: action.frameActorId,
          ...action.data,
        },
        pendingRequestId: null,
        ...action.data,
      };
  }

  return state;
}

/**
 * Retrieve from cache action reducer.
 *
 * @param {Object} state
 * @param {Object} action
 * @returns {Object} new state.
 */
function autoCompleteRetrieveFromCache(state, action) {
  const {input} = action;
  const {cache} = state;

  let filterBy = input;
  if (cache.isElementAccess) {
    // if we're performing an element access, we can simply retrieve whatever comes
    // after the last opening bracket.
    filterBy = input.substring(input.lastIndexOf("[") + 1);
  } else {
    // Find the last non-alphanumeric other than "_", ":", or "$" if it exists.
    const lastNonAlpha = input.match(/[^a-zA-Z0-9_$:][a-zA-Z0-9_$:]*$/);
    // If input contains non-alphanumerics, use the part after the last one
    // to filter the cache.
    if (lastNonAlpha) {
      filterBy = input.substring(input.lastIndexOf(lastNonAlpha) + 1);
    }
  }
  const stripWrappingQuotes = s => s.replace(/^['"`](.+(?=['"`]$))['"`]$/g, "$1");
  const filterByLc = filterBy.toLocaleLowerCase();
  const looseMatching = !filterBy || filterBy[0].toLocaleLowerCase() === filterBy[0];
  const needStripQuote = cache.isElementAccess && !/^[`"']/.test(filterBy);
  const newList = cache.matches.filter(l => {
    if (needStripQuote) {
      l = stripWrappingQuotes(l);
    }

    if (looseMatching) {
      return l.toLocaleLowerCase().startsWith(filterByLc);
    }

    return l.startsWith(filterBy);
  });

  return {
    ...state,
    matches: newList,
    matchProp: filterBy,
    isElementAccess: cache.isElementAccess,
  };
}

exports.autocomplete = autocomplete;
