/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

const initialReducerState = {
  counter: 1,
  breakpoints: [],
};

exports.reducer = domMutationBreakpointReducer;
function domMutationBreakpointReducer(state = initialReducerState, action) {
  switch (action.type) {
    case "ADD_DOM_MUTATION_BREAKPOINT":
      const hasExistingBp = state.breakpoints.some(
        bp =>
          bp.nodeFront === action.nodeFront &&
          bp.mutationType === action.mutationType
      );

      if (hasExistingBp) {
        break;
      }

      state = {
        ...state,
        counter: state.counter + 1,
        breakpoints: [
          ...state.breakpoints,
          {
            id: `${state.counter}`,
            nodeFront: action.nodeFront,
            mutationType: action.mutationType,
            enabled: true,
          },
        ],
      };
      break;
    case "REMOVE_DOM_MUTATION_BREAKPOINT":
      for (const [index, bp] of state.breakpoints.entries()) {
        if (
          bp.nodeFront === action.nodeFront &&
          bp.mutationType === action.mutationType
        ) {
          state = {
            ...state,
            breakpoints: [
              ...state.breakpoints.slice(0, index),
              ...state.breakpoints.slice(index + 1),
            ],
          };
          break;
        }
      }
      break;
    case "REMOVE_DOM_MUTATION_BREAKPOINTS_FOR_FRONTS": {
      const { nodeFronts } = action;
      const nodeFrontSet = new Set(nodeFronts);

      const breakpoints = state.breakpoints.filter(
        bp => !nodeFrontSet.has(bp.nodeFront)
      );

      // Since we might not have made any actual changes, we verify first
      // to avoid unnecessary changes in the state.
      if (state.breakpoints.length !== breakpoints.length) {
        state = {
          ...state,
          breakpoints,
        };
      }
      break;
    }

    case "SET_DOM_MUTATION_BREAKPOINTS_ENABLED_STATE": {
      const { enabledStates } = action;
      const toUpdateById = new Map(enabledStates);

      const breakpoints = state.breakpoints.map(bp => {
        const newBpState = toUpdateById.get(bp.id);
        if (typeof newBpState === "boolean" && newBpState !== bp.enabled) {
          bp = {
            ...bp,
            enabled: newBpState,
          };
        }

        return bp;
      });

      // Since we might not have made any actual changes, we verify first
      // to avoid unnecessary changes in the state.
      if (state.breakpoints.some((bp, i) => breakpoints[i] !== bp)) {
        state = {
          ...state,
          breakpoints,
        };
      }
      break;
    }
  }
  return state;
}

exports.getDOMMutationBreakpoints = getDOMMutationBreakpoints;
function getDOMMutationBreakpoints(state) {
  return state.domMutationBreakpoints.breakpoints;
}

exports.getDOMMutationBreakpoint = getDOMMutationBreakpoint;
function getDOMMutationBreakpoint(state, id) {
  return (
    state.domMutationBreakpoints.breakpoints.find(v => v.id === id) || null
  );
}
