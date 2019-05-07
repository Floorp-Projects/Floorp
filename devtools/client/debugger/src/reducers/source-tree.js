/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Source tree reducer
 * @module reducers/source-tree
 */

import type { SourceTreeAction } from "../actions/types";

export type SourceTreeState = {
  expanded: { [string]: Set<string> }
};

export function InitialState(): SourceTreeState {
  return {
    expanded: {}
  };
}

export default function update(
  state: SourceTreeState = InitialState(),
  action: SourceTreeAction
): SourceTreeState {
  switch (action.type) {
    case "SET_EXPANDED_STATE":
      return updateExpanded(state, action);
  }

  return state;
}

function updateExpanded(state, action) {
  return {
    ...state,
    expanded: { ...state.expanded, [action.thread]: new Set(action.expanded) }
  };
}

type OuterState = {
  sourceTree: SourceTreeState
};

export function getExpandedState(state: OuterState, thread: string) {
  return state.sourceTree.expanded[thread];
}
