/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { actions, viewState } = require("../constants");

const handlers = Object.create(null);

handlers[actions.POP_VIEW] = function (view, _) {
  assert(view.previous, "Had better have a previous view state when POP_VIEW");
  return Object.freeze({
    state: view.previous.state,
    previous: null,
  });
};

handlers[actions.CHANGE_VIEW] = function (view, action) {
  const { newViewState, oldDiffing, oldSelected } = action;
  assert(newViewState);

  if (newViewState === viewState.INDIVIDUALS) {
    assert(oldDiffing || oldSelected);
    return Object.freeze({
      state: newViewState,
      previous: Object.freeze({
        state: view.state,
        selected: oldSelected,
        diffing: oldDiffing,
      }),
    });
  }

  return Object.freeze({
    state: newViewState,
    previous: null,
  });
};

const DEFAULT_VIEW = {
  state: viewState.TREE_MAP,
  previous: null,
};

module.exports = function (view = DEFAULT_VIEW, action) {
  const handler = handlers[action.type];
  return handler ? handler(view, action) : view;
};
