/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Source tree reducer
 * @module reducers/source-tree
 */

export function initialSourcesTreeState() {
  return {
    expanded: new Set(),
    focusedItem: null,
  };
}

export default function update(state = initialSourcesTreeState(), action) {
  switch (action.type) {
    case "SET_EXPANDED_STATE":
      return updateExpanded(state, action);

    case "SET_FOCUSED_SOURCE_ITEM":
      return { ...state, focusedItem: action.item };
  }

  return state;
}

function updateExpanded(state, action) {
  return {
    ...state,
    expanded: new Set(action.expanded),
  };
}

export function getExpandedState(state) {
  return state.sourceTree.expanded;
}

export function getFocusedSourceItem(state) {
  return state.sourceTree.focusedItem;
}
