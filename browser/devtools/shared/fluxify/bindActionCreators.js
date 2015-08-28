/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function bindActionCreator(actionCreator, dispatch) {
  return (...args) => dispatch(actionCreator(...args));
}

/**
 * Wraps action creator functions into a function that automatically
 * dispatches the created action with `dispatch`. Normally action
 * creators simply return actions, but wrapped functions will
 * automatically dispatch.
 *
 * @param {object} actionCreators
 *        An object of action creator functions (names as keys)
 * @param {function} dispatch
 */
function bindActionCreators(actionCreators, dispatch) {
  let actions = {};
  for (let k of Object.keys(actionCreators)) {
    actions[k] = bindActionCreator(actionCreators[k], dispatch);
  }
  return actions;
}

module.exports = bindActionCreators;
