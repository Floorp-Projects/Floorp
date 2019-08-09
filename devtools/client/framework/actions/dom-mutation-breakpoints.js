/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const {
  getDOMMutationBreakpoint,
  getDOMMutationBreakpoints,
} = require("devtools/client/framework/reducers/dom-mutation-breakpoints");

exports.registerWalkerListeners = registerWalkerListeners;
function registerWalkerListeners(toolbox) {
  const { walker } = toolbox;

  walker.on("mutations", mutations =>
    handleWalkerMutations(mutations, toolbox)
  );
}

function handleWalkerMutations(mutations, toolbox) {
  // If we got BP updates for detach/unload, we want to drop those nodes from
  // the list of active DOM mutation breakpoints. We explicitly check these
  // cases because BP updates could also happen due to explicitly API
  // operations to add/remove bps.
  const mutationItems = mutations.filter(
    mutation => mutation.type === "mutationBreakpoint"
  );
  if (mutationItems.length > 0) {
    toolbox.store.dispatch(updateBreakpointsForMutations(mutationItems));
  }
}

exports.createDOMMutationBreakpoint = createDOMMutationBreakpoint;
function createDOMMutationBreakpoint(nodeFront, mutationType) {
  assert(typeof nodeFront === "object" && nodeFront);
  assert(typeof mutationType === "string");

  return async function(dispatch) {
    const walker = nodeFront.parent();

    dispatch({
      type: "ADD_DOM_MUTATION_BREAKPOINT",
      nodeFront,
      mutationType,
    });

    await walker.setMutationBreakpoints(nodeFront, {
      [mutationType]: true,
    });
  };
}

exports.deleteDOMMutationBreakpoint = deleteDOMMutationBreakpoint;
function deleteDOMMutationBreakpoint(nodeFront, mutationType) {
  assert(typeof nodeFront === "object" && nodeFront);
  assert(typeof mutationType === "string");

  return async function(dispatch) {
    dispatch({
      type: "REMOVE_DOM_MUTATION_BREAKPOINT",
      nodeFront,
      mutationType,
    });

    const walker = nodeFront.parent();
    await walker.setMutationBreakpoints(nodeFront, {
      [mutationType]: false,
    });
  };
}

function updateBreakpointsForMutations(mutationItems) {
  return async function(dispatch, getState) {
    const removedNodeFronts = [];
    const changedNodeFronts = new Set();

    for (const { target: nodeFront, mutationReason } of mutationItems) {
      switch (mutationReason) {
        case "api":
          changedNodeFronts.add(nodeFront);
          break;
        default:
          console.error(
            "Unexpected mutation reason",
            mutationReason,
            ", removing"
          );
        // Fall Through
        case "detach":
        case "unload":
          removedNodeFronts.push(nodeFront);
          break;
      }
    }

    if (removedNodeFronts.length > 0) {
      dispatch({
        type: "REMOVE_DOM_MUTATION_BREAKPOINTS_FOR_FRONTS",
        nodeFronts: removedNodeFronts,
      });
    }
    if (changedNodeFronts.length > 0) {
      const enabledStates = [];
      for (const {
        id,
        nodeFront,
        mutationType,
        enabled,
      } of getDOMMutationBreakpoints(getState())) {
        if (changedNodeFronts.has(nodeFront)) {
          const bpEnabledOnFront = nodeFront.mutationBreakpoints[mutationType];
          if (bpEnabledOnFront !== enabled) {
            // Sync the bp state from the front into the store.
            enabledStates.push([id, bpEnabledOnFront]);
          }
        }
      }

      dispatch({
        type: "SET_DOM_MUTATION_BREAKPOINTS_ENABLED_STATE",
        enabledStates,
      });
    }
  };
}

exports.toggleDOMMutationBreakpointState = toggleDOMMutationBreakpointState;
function toggleDOMMutationBreakpointState(id, enabled) {
  assert(typeof id === "string");
  assert(typeof enabled === "boolean");

  return async function(dispatch, getState) {
    const bp = getDOMMutationBreakpoint(getState(), id);
    if (!bp) {
      throw new Error(`No DOM mutation BP with ID ${id}`);
    }

    const walker = bp.nodeFront.parent();
    await walker.setMutationBreakpoints(bp.nodeFront, {
      [bp.mutationType]: enabled,
    });
  };
}
