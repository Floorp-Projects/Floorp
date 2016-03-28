/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require('../constants');
const Immutable = require('devtools/client/shared/vendor/seamless-immutable');
const { mergeIn, setIn, deleteIn } = require('../utils');
const { makeLocationId } = require('../queries');

const initialState = Immutable({
  breakpoints: {}
});

// Return the first argument that is a string, or null if nothing is a
// string.
function firstString(...args) {
  for (var arg of args) {
    if (typeof arg === "string") {
      return arg;
    }
  }
  return null;
}

function update(state = initialState, action, emitChange) {
  switch(action.type) {
  case constants.ADD_BREAKPOINT: {
    const id = makeLocationId(action.breakpoint.location);

    if (action.status === 'start') {
      const existingBp = state.breakpoints[id];
      const bp = existingBp || Immutable(action.breakpoint);

      state = setIn(state, ['breakpoints', id], bp.merge({
        disabled: false,
        loading: true,
        // We want to do an OR here, but we can't because we need
        // empty strings to be truthy, i.e. an empty string is a valid
        // condition.
        condition: firstString(action.condition, bp.condition)
      }));

      emitChange(existingBp ? "breakpoint-enabled" : "breakpoint-added",
                 state.breakpoints[id]);
      return state;
    }
    else if (action.status === 'done') {
      const { actor, text } = action.value;
      let { actualLocation } = action.value;

      // If the breakpoint moved, update the map
      if (actualLocation) {
        // XXX Bug 1227417: The `setBreakpoint` RDP request rdp
        // request returns an `actualLocation` field that doesn't
        // conform to the regular { actor, line } location shape, but
        // it has a `source` field. We should fix that.
        actualLocation = { actor: actualLocation.source.actor,
                           line: actualLocation.line };

        state = deleteIn(state, ['breakpoints', id]);

        const movedId = makeLocationId(actualLocation);
        const currentBp = state.breakpoints[movedId] || Immutable(action.breakpoint);
        const prevLocation = action.breakpoint.location;
        const newBp = currentBp.merge({ location: actualLocation });
        state = setIn(state, ['breakpoints', movedId], newBp);

        emitChange('breakpoint-moved', {
          breakpoint: newBp,
          prevLocation: prevLocation
        });
      }

      const finalLocation = (
        actualLocation ? actualLocation : action.breakpoint.location
      );
      const finalLocationId = makeLocationId(finalLocation);
      state = mergeIn(state, ['breakpoints', finalLocationId], {
        disabled: false,
        loading: false,
        actor: actor,
        text: text
      });
      emitChange('breakpoint-updated', state.breakpoints[finalLocationId]);
      return state;
    }
    else if (action.status === 'error') {
      // Remove the optimistic update
      emitChange('breakpoint-removed', state.breakpoints[id]);
      return deleteIn(state, ['breakpoints', id]);
    }
      break;
  }

  case constants.REMOVE_BREAKPOINT: {
    if (action.status === 'done') {
      const id = makeLocationId(action.breakpoint.location);
      const bp = state.breakpoints[id];

      if (action.disabled) {
        state = mergeIn(state, ['breakpoints', id],
                        { loading: false, disabled: true });
        emitChange('breakpoint-disabled', state.breakpoints[id]);
        return state;
      }

      state = deleteIn(state, ['breakpoints', id]);
      emitChange('breakpoint-removed', bp);
      return state;
    }
    break;
  }

  case constants.SET_BREAKPOINT_CONDITION: {
    const id = makeLocationId(action.breakpoint.location);
    const bp = state.breakpoints[id];
    emitChange("breakpoint-condition-updated", bp);

    if (!action.status) {
      // No status means that it wasn't a remote request. Just update
      // the condition locally.
      return mergeIn(state, ['breakpoints', id], {
        condition: action.condition
      });
    }
    else if (action.status === 'start') {
      return mergeIn(state, ['breakpoints', id], {
        loading: true,
        condition: action.condition
      });
    }
    else if (action.status === 'done') {
      return mergeIn(state, ['breakpoints', id], {
        loading: false,
        condition: action.condition,
        // Setting a condition creates a new breakpoint client as of
        // now, so we need to update the actor
        actor: action.value.actor
      });
    }
    else if (action.status === 'error') {
      emitChange("breakpoint-removed", bp);
      return deleteIn(state, ['breakpoints', id]);
    }

    break;
  }}

  return state;
}

module.exports = update;
