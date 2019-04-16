/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  AUTOCOMPLETE_CLEAR,
  AUTOCOMPLETE_DATA_RECEIVE,
  AUTOCOMPLETE_PENDING_REQUEST,
  AUTOCOMPLETE_RETRIEVE_FROM_CACHE,
  APPEND_TO_HISTORY,
  UPDATE_HISTORY_POSITION,
  REVERSE_SEARCH_INPUT_CHANGE,
  REVERSE_SEARCH_BACK,
  REVERSE_SEARCH_NEXT,
  WILL_NAVIGATE,
} = require("devtools/client/webconsole/constants");

function getDefaultState(overrides = {}) {
  return Object.freeze({
    cache: null,
    matches: [],
    matchProp: null,
    isElementAccess: false,
    pendingRequestId: null,
    isUnsafeGetter: false,
    getterPath: null,
    authorizedEvaluations: [],
    ...overrides,
  });
}

function autocomplete(state = getDefaultState(), action) {
  switch (action.type) {
    case WILL_NAVIGATE:
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

      if (action.data.isUnsafeGetter) {
        // We only want to display the getter confirm popup if the last char is a dot or
        // an opening bracket, or if the user forced the autocompletion with Ctrl+Space.
        if (action.input.endsWith(".") || action.input.endsWith("[") || action.force) {
          return {
            ...getDefaultState(),
            isUnsafeGetter: true,
            getterPath: action.data.getterPath,
            authorizedEvaluations: action.authorizedEvaluations,
          };
        }

        return {
          ...state,
          pendingRequestId: null,
        };
      }

      return {
        ...state,
        authorizedEvaluations: action.authorizedEvaluations,
        getterPath: null,
        isUnsafeGetter: false,
        pendingRequestId: null,
        cache: {
          input: action.input,
          frameActorId: action.frameActorId,
          ...action.data,
        },
        ...action.data,
      };
    // Reset the autocomplete data when:
    // - clear is explicitely called
    // - the user navigates the history
    // - or an item was added to the history (i.e. something was evaluated).
    case AUTOCOMPLETE_CLEAR:
      return getDefaultState({
        authorizedEvaluations: state.authorizedEvaluations,
      });
    case APPEND_TO_HISTORY:
    case UPDATE_HISTORY_POSITION:
    case REVERSE_SEARCH_INPUT_CHANGE:
    case REVERSE_SEARCH_BACK:
    case REVERSE_SEARCH_NEXT:
      return getDefaultState();
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

  newList.sort((a, b) => {
    const startingQuoteRegex = /^('|"|`)/;
    const aFirstMeaningfulChar = startingQuoteRegex.test(a) ? a[1] : a[0];
    const bFirstMeaningfulChar = startingQuoteRegex.test(b) ? b[1] : b[0];
    const lA = aFirstMeaningfulChar.toLocaleLowerCase() === aFirstMeaningfulChar;
    const lB = bFirstMeaningfulChar.toLocaleLowerCase() === bFirstMeaningfulChar;
    if (lA === lB) {
      if (a === filterBy) {
        return -1;
      }
      if (b === filterBy) {
        return 1;
      }
      return a.localeCompare(b);
    }
    return lA ? -1 : 1;
  });

  return {
    ...state,
    isUnsafeGetter: false,
    getterPath: null,
    matches: newList,
    matchProp: filterBy,
    isElementAccess: cache.isElementAccess,
  };
}

exports.autocomplete = autocomplete;
