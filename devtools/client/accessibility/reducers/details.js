/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { UPDATE_DETAILS, RESET } = require("../constants");

/**
 * Initial state definition
 */
function getInitialState() {
  return {};
}

/**
 * Maintain details of a current relevant accessible.
 */
function details(state = getInitialState(), action) {
  switch (action.type) {
    case UPDATE_DETAILS:
      return onUpdateDetails(state, action);
    case RESET:
      return getInitialState();
    default:
      return state;
  }
}

/**
 * Handle details update for an accessible object
 * @param {Object} state  Current accessible object details.
 * @param {Object} action Redux action object
 * @return {Object}  updated state
 */
function onUpdateDetails(state, action) {
  const { accessible, response: DOMNode, error } = action;
  if (error) {
    console.warn("Error fetching DOMNode for accessible", accessible, error);
    return state;
  }

  return { accessible, DOMNode };
}

exports.details = details;
