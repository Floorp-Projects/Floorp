/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require('../constants');
const Immutable = require('devtools/client/shared/vendor/seamless-immutable');
const { mergeIn, setIn } = require('../utils');

const initialState = Immutable({
  sources: {},
  selectedSource: null,
  selectedSourceOpts: null,
  sourcesText: {}
});

function update(state = initialState, action, emitChange) {
  switch(action.type) {
  case constants.ADD_SOURCE:
    emitChange('source', action.source);
    return mergeIn(state, ['sources', action.source.actor], action.source);

  case constants.LOAD_SOURCES:
    if (action.status === "done") {
      const sources = action.value;
      if (!sources) {
        return state;
      }
      const sourcesByActor = {};
      sources.forEach(source => {
        if (!state.sources[source.actor]) {
          emitChange('source', source);
        }
        sourcesByActor[source.actor] = source;
      });
      return mergeIn(state, ['sources'], state.sources.merge(sourcesByActor))
    }
    break;

  case constants.SELECT_SOURCE:
    emitChange('source-selected', action.source);
    return state.merge({
      selectedSource: action.source.actor,
      selectedSourceOpts: action.opts
    });

  case constants.LOAD_SOURCE_TEXT: {
    const s = _updateText(state, action);
    emitChange('source-text-loaded', s.sources[action.source.actor]);
    return s;
  }

  case constants.BLACKBOX:
    if (action.status === 'done') {
      const s = mergeIn(state,
                        ['sources', action.source.actor, 'isBlackBoxed'],
                        action.value.isBlackBoxed);
      emitChange('blackboxed', s.sources[action.source.actor]);
      return s;
    }
    break;

  case constants.TOGGLE_PRETTY_PRINT:
    let s = state;
    if (action.status === "error") {
      s = mergeIn(state, ['sourcesText', action.source.actor], {
        loading: false
      });

      // If it errored, just display the source as it way before.
      emitChange('prettyprinted', s.sources[action.source.actor]);
    }
    else {
      s = _updateText(state, action);
      // Don't do this yet, the progress bar is still imperatively shown
      // from the source view. We will fix in the next iteration.
      // emitChange('source-text-loaded', s.sources[action.source.actor]);

      if (action.status === 'done') {
        s = mergeIn(s,
                    ['sources', action.source.actor, 'isPrettyPrinted'],
                    action.value.isPrettyPrinted);
        emitChange('prettyprinted', s.sources[action.source.actor]);
      }
    }
    return s;

  case constants.UNLOAD:
    // Reset the entire state to just the initial state, a blank state
    // if you will.
    return initialState;
  }

  return state;
}

function _updateText(state, action) {
  const { source } = action;

  if (action.status === 'start') {
    // Merge this in, don't set it. That way the previous value is
    // still stored here, and we can retrieve it if whatever we're
    // doing fails.
    return mergeIn(state, ['sourcesText', source.actor], {
      loading: true
    });
  }
  else if (action.status === 'error') {
    return setIn(state, ['sourcesText', source.actor], {
      error: action.error
    });
  }
  else {
    return setIn(state, ['sourcesText', source.actor], {
      text: action.value.text,
      contentType: action.value.contentType
    });
  }
}

module.exports = update;
