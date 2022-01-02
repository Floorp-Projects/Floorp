/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals DomProvider */
"use strict";

const constants = require("devtools/client/dom/content/constants");

/**
 * Used to fetch grip prototype and properties from the backend.
 */
function requestProperties(grip) {
  return {
    grip: grip,
    type: constants.FETCH_PROPERTIES,
    status: "start",
    error: false,
  };
}

/**
 * Executed when grip properties are received from the backend.
 */
function receiveProperties(grip, response, error) {
  return {
    grip: grip,
    type: constants.FETCH_PROPERTIES,
    status: "done",
    response: response,
    error: error,
  };
}

/**
 * Used to get properties from the backend and fire an action
 * when they are received.
 */
function fetchProperties(grip) {
  return async ({ dispatch }) => {
    try {
      // Use 'DomProvider' object exposed from the chrome scope.
      const response = await DomProvider.getPrototypeAndProperties(grip);
      dispatch(receiveProperties(grip, response));
    } catch (e) {
      console.error("Error while fetching properties", e);
    }
    DomProvider.onPropertiesFetched();
  };
}

// Exports from this module
exports.requestProperties = requestProperties;
exports.receiveProperties = receiveProperties;
exports.fetchProperties = fetchProperties;
